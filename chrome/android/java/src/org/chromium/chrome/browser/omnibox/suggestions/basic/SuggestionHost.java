// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism for creating {@link SuggestionViewDelegate}s. */
public interface SuggestionHost {
    /**
     * @param suggestion The suggestion to create the delegate for.
     * @param position The position of the delegate in the list.
     * @return A delegate for the specified suggestion.
     */
    SuggestionViewDelegate createSuggestionViewDelegate(OmniboxSuggestion suggestion, int position);

    /**
     * @param model The model to check.
     * @return Whether the model is active in the list being shown.
     */
    boolean isActiveModel(PropertyModel model);

    /**
     * Notify the host that the suggestion models have changed.
     */
    void notifyPropertyModelsChanged();
}
