// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private final SuggestionHost mSuggestionHost;

    /**
     * @param host A handle to the object using the suggestions.
     */
    public BaseSuggestionViewProcessor(SuggestionHost host) {
        mSuggestionHost = host;
    }

    /**
     * @return whether suggestion can be refined and a refine icon should be shown.
     */
    protected boolean canRefine(OmniboxSuggestion suggestion) {
        return true;
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        model.set(BaseSuggestionViewProperties.SUGGESTION_DELEGATE, delegate);
        model.set(BaseSuggestionViewProperties.REFINE_VISIBLE, canRefine(suggestion));
    }
}
