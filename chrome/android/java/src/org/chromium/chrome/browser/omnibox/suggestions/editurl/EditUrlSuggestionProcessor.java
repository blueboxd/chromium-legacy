// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with
 * the rest of Chrome.
 */
public class EditUrlSuggestionProcessor implements OnClickListener, SuggestionProcessor {
    /** An interface for handling taps on the suggestion rather than its buttons. */
    public interface SuggestionSelectionHandler {
        /**
         * Handle the edit URL suggestion selection.
         * @param suggestion The selected suggestion.
         */
        void onEditUrlSuggestionSelected(OmniboxSuggestion suggestion);
    }

    /** An interface for modifying the location bar and it's contents. */
    public interface LocationBarDelegate {
        /** Remove focus from the omnibox. */
        void clearOmniboxFocus();

        /**
         * Set the text in the omnibox.
         * @param text The text that should be displayed in the omnibox.
         */
        void setOmniboxEditingText(String text);
    }

    /** The actions that can be performed on the suggestion view provided by this class. */
    @IntDef({SuggestionAction.EDIT, SuggestionAction.COPY, SuggestionAction.SHARE,
            SuggestionAction.TAP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionAction {
        int EDIT = 0;
        int COPY = 1;
        int SHARE = 2;
        int TAP = 3;
        int NUM_ENTRIES = 4;
    }

    /** Cached metrics in the event this code is triggered prior to native being initialized. */
    private static final EnumeratedHistogramSample ENUMERATED_SUGGESTION_ACTION =
            new EnumeratedHistogramSample(
                    "Omnibox.EditUrlSuggestionAction", SuggestionAction.NUM_ENTRIES);
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_TAP =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Tap");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_COPY =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Copy");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_EDIT =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Edit");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_SHARE =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Share");

    /** The delegate for accessing the location bar for observation and modification. */
    private final LocationBarDelegate mLocationBarDelegate;

    /** A means of accessing the activity's tab. */
    private ActivityTabProvider mTabProvider;

    /** Whether the omnibox has already cleared its content for the focus event. */
    private boolean mHasClearedOmniboxForFocus;

    /** The last omnibox suggestion to be processed. */
    private OmniboxSuggestion mLastProcessedSuggestion;

    /** A handler for suggestion selection. */
    private SuggestionSelectionHandler mSelectionHandler;

    /** The original title of the page. */
    private String mOriginalTitle;

    /** Whether this processor should ignore all subsequent suggestion. */
    private boolean mIgnoreSuggestions;

    /** Whether suggestion site favicons are enabled. */
    private boolean mEnableSuggestionFavicons;

    /** Edge size (in pixels) of the favicon. Used to request best matching favicon from cache. */
    private final int mDesiredFaviconWidthPx;

    /** Supplies site favicons. */
    private final Supplier<LargeIconBridge> mIconBridgeSupplier;

    /** Supplies additional control over suggestion model. */
    private final SuggestionHost mSuggestionHost;

    /**
     * @param locationBarDelegate A means of modifying the location bar.
     * @param selectionHandler A mechanism for handling selection of the edit URL suggestion item.
     */
    public EditUrlSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            LocationBarDelegate locationBarDelegate, SuggestionSelectionHandler selectionHandler,
            Supplier<LargeIconBridge> iconBridgeSupplier) {
        mLocationBarDelegate = locationBarDelegate;
        mSelectionHandler = selectionHandler;
        mDesiredFaviconWidthPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mSuggestionHost = suggestionHost;
        mIconBridgeSupplier = iconBridgeSupplier;
    }

    /**
     * Create the view specific to the suggestion this processor is responsible for.
     * @param context An Android context.
     * @return An edit-URL suggestion view.
     */
    public static ViewGroup createView(Context context) {
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        return (ViewGroup) inflater.inflate(R.layout.edit_url_suggestion_layout, null);
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        Tab activeTab = mTabProvider != null ? mTabProvider.get() : null;
        // The what-you-typed suggestion can potentially appear as the second suggestion in some
        // cases. If the first suggestion isn't the one we want, ignore all subsequent suggestions.
        if (activeTab == null || activeTab.isNativePage() || activeTab.isIncognito()
                || SadTab.isShowing(activeTab)) {
            return false;
        }

        mLastProcessedSuggestion = suggestion;

        if (!isSuggestionEquivalentToCurrentPage(mLastProcessedSuggestion, activeTab.getUrl())) {
            return false;
        }

        if (!mHasClearedOmniboxForFocus) {
            mHasClearedOmniboxForFocus = true;
            mLocationBarDelegate.setOmniboxEditingText("");
        }
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.EDIT_URL_SUGGESTION;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return new PropertyModel(EditUrlSuggestionProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        model.set(EditUrlSuggestionProperties.TEXT_CLICK_LISTENER, this);
        model.set(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER, this);
        if (mOriginalTitle == null) mOriginalTitle = mTabProvider.get().getTitle();
        model.set(EditUrlSuggestionProperties.TITLE_TEXT, mOriginalTitle);
        model.set(EditUrlSuggestionProperties.URL_TEXT, mLastProcessedSuggestion.getUrl());
        fetchIcon(model, mLastProcessedSuggestion.getUrl());
    }

    private void fetchIcon(PropertyModel model, String url) {
        if (!mEnableSuggestionFavicons || url == null) return;

        final LargeIconBridge iconBridge = mIconBridgeSupplier.get();
        if (iconBridge == null) return;

        iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                        int iconType) -> model.set(EditUrlSuggestionProperties.SITE_FAVICON, icon));
    }

    @Override
    public void onNativeInitialized() {
        mEnableSuggestionFavicons =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS);
    }

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {}

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {}

    /**
     * @param provider A means of accessing the activity's tab.
     */
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mTabProvider = provider;
    }

    /**
     * Clean up any state that this coordinator has.
     */
    public void destroy() {
        mLastProcessedSuggestion = null;
        mSelectionHandler = null;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) return;

        mOriginalTitle = null;
        mHasClearedOmniboxForFocus = false;
        mLastProcessedSuggestion = null;
        mIgnoreSuggestions = false;
    }

    @Override
    public void onClick(View view) {
        Tab activityTab = mTabProvider.get();
        assert activityTab != null : "A tab is required to make changes to the location bar.";

        if (R.id.url_copy_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.COPY);
            ACTION_EDIT_URL_SUGGESTION_COPY.record();
            Clipboard.getInstance().copyUrlToClipboard(mLastProcessedSuggestion.getUrl());
        } else if (R.id.url_share_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.SHARE);
            ACTION_EDIT_URL_SUGGESTION_SHARE.record();
            mLocationBarDelegate.clearOmniboxFocus();
            // TODO(mdjones): This should only share the displayed URL instead of the background
            //                tab.
            ((TabImpl) activityTab)
                    .getActivity()
                    .getShareDelegateSupplier()
                    .get()
                    .share(activityTab, false);
        } else if (R.id.url_edit_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.EDIT);
            ACTION_EDIT_URL_SUGGESTION_EDIT.record();
            mLocationBarDelegate.setOmniboxEditingText(mLastProcessedSuggestion.getUrl());
        } else {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.TAP);
            ACTION_EDIT_URL_SUGGESTION_TAP.record();
            // If the event wasn't on any of the buttons, treat is as a tap on the general
            // suggestion.
            if (mSelectionHandler != null) {
                mSelectionHandler.onEditUrlSuggestionSelected(mLastProcessedSuggestion);
            }
        }
    }

    /**
     * @return true if the suggestion is effectively the same as the current page, either because:
     * 1. It's a search suggestion for the same search terms as the current SERP.
     * 2. It's a URL suggestion for the current URL.
     */
    private boolean isSuggestionEquivalentToCurrentPage(
            OmniboxSuggestion suggestion, String pageUrl) {
        switch (suggestion.getType()) {
            case OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED:
                return TextUtils.equals(suggestion.getFillIntoEdit(),
                        TemplateUrlServiceFactory.get().getSearchQueryForUrl(pageUrl));
            case OmniboxSuggestionType.URL_WHAT_YOU_TYPED:
                return TextUtils.equals(suggestion.getUrl(), pageUrl);
            default:
                return false;
        }
    }
}
