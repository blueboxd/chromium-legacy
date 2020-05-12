// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link AutocompleteResult}.
 */
public class AutocompleteResultUnitTest {
    @CalledByNative
    private AutocompleteResultUnitTest() {}

    private OmniboxSuggestion buildSuggestionForIndex(int index) {
        return OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Dummy Suggestion " + index)
                .setDescription("Dummy Description " + index)
                .build();
    }

    @CalledByNativeJavaTest
    public void autocompleteResult_sameContentsAreEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<String> headers1 = new SparseArray<>();
        SparseArray<String> headers2 = new SparseArray<>();

        headers1.put(10, "Hello");
        headers1.put(20, "Test");

        headers2.put(10, "Hello");
        headers2.put(20, "Test");

        AutocompleteResult res1 = new AutocompleteResult(list1, headers1);
        AutocompleteResult res2 = new AutocompleteResult(list2, headers2);

        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @CalledByNativeJavaTest
    public void autocompleteResult_itemsOutOfOrderAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(2), buildSuggestionForIndex(1), buildSuggestionForIndex(3));

        SparseArray<String> headers1 = new SparseArray<>();
        SparseArray<String> headers2 = new SparseArray<>();

        headers1.put(10, "Hello");
        headers1.put(20, "Test");

        headers2.put(10, "Hello");
        headers2.put(20, "Test");

        AutocompleteResult res1 = new AutocompleteResult(list1, headers1);
        AutocompleteResult res2 = new AutocompleteResult(list2, headers2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @CalledByNativeJavaTest
    public void autocompleteResult_missingGroupHeadersAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<String> headers1 = new SparseArray<>();
        SparseArray<String> headers2 = new SparseArray<>();

        headers1.put(10, "Hello");
        headers1.put(20, "Test");

        headers2.put(10, "Hello");

        AutocompleteResult res1 = new AutocompleteResult(list1, headers1);
        AutocompleteResult res2 = new AutocompleteResult(list2, headers2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @CalledByNativeJavaTest
    public void autocompleteResult_extraGroupHeadersAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<String> headers1 = new SparseArray<>();
        SparseArray<String> headers2 = new SparseArray<>();

        headers1.put(10, "Hello");
        headers1.put(20, "Test");

        headers2.put(10, "Hello");
        headers2.put(20, "Test");
        headers2.put(30, "Yikes");

        AutocompleteResult res1 = new AutocompleteResult(list1, headers1);
        AutocompleteResult res2 = new AutocompleteResult(list2, headers2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }
}
