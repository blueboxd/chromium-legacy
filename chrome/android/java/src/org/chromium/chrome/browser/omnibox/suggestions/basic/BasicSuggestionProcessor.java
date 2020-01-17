// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.DrawableRes;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A class that handles model and view creation for the basic omnibox suggestions. */
public class BasicSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final Context mContext;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final Supplier<LargeIconBridge> mIconBridgeSupplier;
    private boolean mEnableSuggestionFavicons;
    private final int mDesiredFaviconWidthPx;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     */
    public BasicSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider,
            Supplier<LargeIconBridge> iconBridgeSupplier) {
        super(context, suggestionHost);

        mContext = context;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mUrlBarEditingTextProvider = editingTextProvider;
        mIconBridgeSupplier = iconBridgeSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.DEFAULT;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.IconOrFaviconShown",
                model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE),
                SuggestionIcon.TOTAL_COUNT);
    }

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.SuggestionUsed.IconOrFaviconType",
                model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE),
                SuggestionIcon.TOTAL_COUNT);
    }

    /**
     * Signals that native initialization has completed.
     */
    @Override
    public void onNativeInitialized() {
        // Experiment: controls presence of certain answer icon types.
        mEnableSuggestionFavicons =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS);
    }

    /** Decide whether suggestion should receive a refine arrow. */
    @Override
    protected boolean canRefine(OmniboxSuggestion suggestion) {
        final @OmniboxSuggestionType int suggestionType = suggestion.getType();

        if (suggestionType == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestionType == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestionType == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
            return false;
        }

        return !mUrlBarEditingTextProvider.getTextWithoutAutocomplete().trim().equalsIgnoreCase(
                suggestion.getDisplayText());
    }

    /**
     * Returns suggestion icon to be presented for specified omnibox suggestion.
     *
     * This method returns the stock icon type to be attached to the Suggestion.
     * Note that the stock icons do not include Favicon - Favicon is only declared
     * when we know we have a valid and large enough site favicon to present.
     */
    private @SuggestionIcon int getSuggestionIconType(OmniboxSuggestion suggestion) {
        if (suggestion.isUrlSuggestion()) {
            if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                    || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                return SuggestionIcon.MAGNIFIER;
            } else if (suggestion.isStarred()) {
                return SuggestionIcon.BOOKMARK;
            } else {
                return SuggestionIcon.GLOBE;
            }
        } else /* Search suggestion */ {
            switch (suggestion.getType()) {
                case OmniboxSuggestionType.VOICE_SUGGEST:
                    return SuggestionIcon.VOICE;

                case OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED:
                case OmniboxSuggestionType.SEARCH_HISTORY:
                    return SuggestionIcon.HISTORY;

                default:
                    return SuggestionIcon.MAGNIFIER;
            }
        }
    }

    private void updateSuggestionIcon(OmniboxSuggestion suggestion, PropertyModel model) {
        if (!(mEnableSuggestionFavicons
                    || DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext))) {
            return;
        }

        @SuggestionIcon
        int type = getSuggestionIconType(suggestion);
        @DrawableRes
        int icon = R.drawable.ic_suggestion_magnifier;

        switch (type) {
            case SuggestionIcon.BOOKMARK:
                icon = R.drawable.btn_star;
                break;

            case SuggestionIcon.HISTORY:
                icon = R.drawable.ic_history_googblue_24dp;
                break;

            case SuggestionIcon.GLOBE:
                icon = R.drawable.ic_globe_24dp;
                break;

            case SuggestionIcon.MAGNIFIER:
                icon = R.drawable.ic_suggestion_magnifier;
                break;

            case SuggestionIcon.VOICE:
                icon = R.drawable.btn_mic;
                break;

            default:
                // All other cases are invalid.
                assert false : "Suggestion type " + type + " is not valid.";
        }

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, type);
        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forDrawableRes(mContext, icon)
                        .setAllowTint(true)
                        .build());
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        final @OmniboxSuggestionType int suggestionType = suggestion.getType();
        SuggestionSpannable textLine2 = null;
        boolean urlHighlighted = false;

        if (suggestion.isUrlSuggestion()) {
            if (!TextUtils.isEmpty(suggestion.getUrl())) {
                SuggestionSpannable str = new SuggestionSpannable(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
            }
        } else if (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE) {
            textLine2 = new SuggestionSpannable(suggestion.getDescription());
        }

        final SuggestionSpannable textLine1 =
                getSuggestedQuery(suggestion, suggestion.isUrlSuggestion(), !urlHighlighted);

        updateSuggestionIcon(suggestion, model);
        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION,
                !suggestion.isUrlSuggestion()
                        || suggestionType == OmniboxSuggestionType.CLIPBOARD_IMAGE
                        || suggestionType == OmniboxSuggestionType.CLIPBOARD_TEXT);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);
        fetchSuggestionFavicon(model, suggestion.getUrl(), suggestion.getType());
    }

    /**
     * Fetch suggestion favicon, if one is available.
     * Updates icon decoration in supplied |model| if |url| is not null and points to an already
     * visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     * @param type Suggestion type.
     */
    private void fetchSuggestionFavicon(
            PropertyModel model, String url, @OmniboxSuggestionType int type) {
        if (!mEnableSuggestionFavicons || url == null
                || type == OmniboxSuggestionType.CLIPBOARD_TEXT) {
            return;
        }

        // Include site favicon if we are presenting URL and have favicon available.
        // TODO(gangwu): Create a separate processor for clipboard suggestions.
        final LargeIconBridge iconBridge = mIconBridgeSupplier.get();
        if (iconBridge == null) return;

        iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) -> {
                    if (icon == null) return;

                    setSuggestionDrawableState(
                            model, SuggestionDrawableState.Builder.forBitmap(icon).build());
                    model.set(
                            SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.FAVICON);
                });
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private SuggestionSpannable getSuggestedQuery(OmniboxSuggestion suggestion,
            boolean showDescriptionIfPresent, boolean shouldHighlight) {
        String userQuery = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        String suggestedQuery = null;
        List<OmniboxSuggestion.MatchClassification> classifications;
        if (showDescriptionIfPresent && !TextUtils.isEmpty(suggestion.getUrl())
                && !TextUtils.isEmpty(suggestion.getDescription())) {
            suggestedQuery = suggestion.getDescription();
            classifications = suggestion.getDescriptionClassifications();
        } else {
            suggestedQuery = suggestion.getDisplayText();
            classifications = suggestion.getDisplayTextClassifications();
        }
        if (suggestedQuery == null) {
            assert false : "Invalid suggestion sent with no displayable text";
            suggestedQuery = "";
            classifications = new ArrayList<OmniboxSuggestion.MatchClassification>();
            classifications.add(
                    new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
        }

        SuggestionSpannable str = new SuggestionSpannable(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }
}
