// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.List;

/**
 * AutocompleteResult encompasses and manages autocomplete results.
 */
public class AutocompleteResult {
    private final List<OmniboxSuggestion> mSuggestions;
    private final SparseArray<String> mGroupHeaders;

    public AutocompleteResult(
            List<OmniboxSuggestion> suggestions, SparseArray<String> groupHeaders) {
        mSuggestions = suggestions != null ? suggestions : new ArrayList<>();
        mGroupHeaders = groupHeaders != null ? groupHeaders : new SparseArray<>();
    }

    /**
     * @return List of Omnibox Suggestions.
     */
    @NonNull
    List<OmniboxSuggestion> getSuggestionsList() {
        return mSuggestions;
    }

    /**
     * @return Map of Group ID to Header text
     */
    @NonNull
    SparseArray<String> getGroupHeaders() {
        return mGroupHeaders;
    }
}
