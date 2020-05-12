// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.core.util.ObjectsCompat;

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

    @Override
    public boolean equals(Object otherObj) {
        if (otherObj == this) return true;
        if (!(otherObj instanceof AutocompleteResult)) return false;

        AutocompleteResult other = (AutocompleteResult) otherObj;
        if (!mSuggestions.equals(other.mSuggestions)) return false;

        final SparseArray<String> otherHeaders = other.mGroupHeaders;
        if (mGroupHeaders.size() != otherHeaders.size()) return false;
        for (int index = 0; index < mGroupHeaders.size(); index++) {
            if (mGroupHeaders.keyAt(index) != otherHeaders.keyAt(index)) return false;
            if (!ObjectsCompat.equals(mGroupHeaders.valueAt(index), otherHeaders.valueAt(index))) {
                return false;
            }
        }

        return true;
    }

    @Override
    public int hashCode() {
        int baseHash = 0;
        for (int index = 0; index < mGroupHeaders.size(); index++) {
            baseHash += mGroupHeaders.keyAt(index);
            baseHash ^= mGroupHeaders.valueAt(index).hashCode();
            baseHash = (baseHash << 10) | (baseHash >> 22);
        }
        return baseHash ^ mSuggestions.hashCode();
    }
}
