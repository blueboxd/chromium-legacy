// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private static final String SUGGESTION_DENSITY_PARAM = "omnibox_compact_suggestions_variant";
    private static final String SUGGESTION_DENSITY_SEMICOMPACT = "semi-compact";
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;
    private final int mDesiredFaviconWidthPx;
    private int mSuggestionSizePx;
    private @BaseSuggestionViewProperties.Density int mDensity =
            BaseSuggestionViewProperties.Density.COMFORTABLE;

    /**
     * @param context Current context.
     * @param host A handle to the object using the suggestions.
     */
    public BaseSuggestionViewProcessor(Context context, SuggestionHost host) {
        mContext = context;
        mSuggestionHost = host;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_comfortable_height);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {}

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {}

    @Override
    public void onNativeInitialized() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)) {
            if (SUGGESTION_DENSITY_SEMICOMPACT.equals(ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS, SUGGESTION_DENSITY_PARAM))) {
                mDensity = BaseSuggestionViewProperties.Density.SEMICOMPACT;
                mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_semicompact_height);
            } else {
                mDensity = BaseSuggestionViewProperties.Density.COMPACT;
                mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_compact_height);
            }
        }
    }

    @Override
    public void onSuggestionsReceived() {}

    @Override
    public int getMinimumSuggestionViewHeight() {
        return mSuggestionSizePx;
    }

    /**
     * Specify SuggestionDrawableState for suggestion decoration.
     *
     * @param decoration SuggestionDrawableState object defining decoration for the suggestion.
     */
    protected void setSuggestionDrawableState(
            PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify SuggestionDrawableState for action button.
     *
     * @param model Property model to update.
     * @param drawable SuggestionDrawableState object defining decoration for the action button.
     * @param callback Runnable to invoke when user presses the action icon.
     */
    protected void setCustomAction(
            PropertyModel model, SuggestionDrawableState drawable, Runnable callback) {
        model.set(BaseSuggestionViewProperties.ACTION_ICON, drawable);
        model.set(BaseSuggestionViewProperties.ACTION_CALLBACK, callback);
    }

    /**
     * Specify data for Refine action.
     *
     * @param model Property model to update.
     * @param refineText Text to update Omnibox with when Refine button is pressed.
     * @param isSearchQuery Whether refineText is an URL or Search query.
     */
    protected void setRefineAction(PropertyModel model, OmniboxSuggestion suggestion) {
        setCustomAction(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(mContext, R.drawable.btn_suggestion_refine)
                        .setLarge(true)
                        .setAllowTint(true)
                        .build(),
                () -> { mSuggestionHost.onRefineSuggestion(suggestion); });
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        model.set(BaseSuggestionViewProperties.SUGGESTION_DELEGATE, delegate);
        model.set(BaseSuggestionViewProperties.DENSITY, mDensity);
        setCustomAction(model, null, null);
    }

    /**
     * Apply In-Place highlight to matching sections of Suggestion text.
     *
     * @param text Suggestion text to apply highlight to.
     * @param classifications Classifications describing how to format text.
     * @return true, if at least one highlighted match section was found.
     */
    protected static boolean applyHighlightToMatchRegions(
            Spannable text, List<MatchClassification> classifications) {
        if (text == null || classifications == null) return false;

        boolean hasAtLeastOneMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = text.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, text.length());
                matchEndIndex = Math.min(matchEndIndex, text.length());

                hasAtLeastOneMatch = true;
                // Bold the part of the URL that matches the user query.
                text.setSpan(new StyleSpan(Typeface.BOLD), matchStartIndex, matchEndIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasAtLeastOneMatch;
    }

    /**
     * Fetch suggestion favicon, if one is available.
     * Updates icon decoration in supplied |model| if |url| is not null and points to an already
     * visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     * @param iconBridge A {@link LargeIconBridge} supplies site favicons.
     * @param onIconFetched Optional callback that will be invoked after successful fetch of a
     *         favicon.
     */
    protected void fetchSuggestionFavicon(PropertyModel model, GURL url, LargeIconBridge iconBridge,
            @Nullable Runnable onIconFetched) {
        if (url == null || iconBridge == null) return;

        iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) -> {
                    if (icon == null) return;

                    setSuggestionDrawableState(model,
                            SuggestionDrawableState.Builder.forBitmap(mContext, icon).build());
                    if (onIconFetched != null) {
                        onIconFetched.run();
                    }
                });
    }
}
