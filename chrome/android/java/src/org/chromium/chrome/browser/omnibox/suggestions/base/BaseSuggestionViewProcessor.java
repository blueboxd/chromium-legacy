// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;

    /**
     * @param host A handle to the object using the suggestions.
     */
    public BaseSuggestionViewProcessor(Context context, SuggestionHost host) {
        mContext = context;
        mSuggestionHost = host;
    }

    /**
     * @return whether suggestion can be refined and a refine icon should be shown.
     */
    protected boolean canRefine(OmniboxSuggestion suggestion) {
        return true;
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
     * @param decoration SuggestionDrawableState object defining decoration for the action button.
     */
    protected void setActionDrawableState(PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ACTION_ICON, decoration);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        model.set(BaseSuggestionViewProperties.SUGGESTION_DELEGATE, delegate);

        if (canRefine(suggestion)) {
            setActionDrawableState(model,
                    SuggestionDrawableState.Builder
                            .forDrawableRes(mContext, R.drawable.btn_suggestion_refine)
                            .setLarge(true)
                            .setAllowTint(true)
                            .build());
        } else {
            setActionDrawableState(model, null);
        }
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
}
