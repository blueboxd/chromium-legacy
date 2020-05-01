// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.text.TextUtils;
import android.util.SparseArray;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link OmniboxSuggestion}.
 */
public class OmniboxSuggestionUnitTest {
    @CalledByNative
    private OmniboxSuggestionUnitTest() {}

    /**
     * Compare two cached {@link OmniboxSuggestion} to see if they are same. Only comparing cached
     * fields here Since OmniboxSuggestion::cacheOmniboxSuggestionListForZeroSuggest do not cache
     * all the fields.
     *
     * @return True if two {@link OmniboxSuggestion} are same, otherwise false.
     */
    boolean isCachedOmniboxSuggestionMatch(
            OmniboxSuggestion suggestion1, OmniboxSuggestion suggestion2) {
        return suggestion1.getType() == suggestion2.getType()
                && TextUtils.equals(suggestion1.getDisplayText(), suggestion2.getDisplayText())
                && TextUtils.equals(suggestion1.getDescription(), suggestion2.getDescription())
                && suggestion1.getUrl().equals(suggestion2.getUrl())
                && suggestion1.isSearchSuggestion() == suggestion2.isSearchSuggestion()
                && suggestion1.isStarred() == suggestion2.isStarred()
                && suggestion1.isDeletable() == suggestion2.isDeletable()
                && TextUtils.equals(
                        suggestion1.getPostContentType(), suggestion2.getPostContentType())
                && Arrays.equals(suggestion1.getPostData(), suggestion2.getPostData());
    }

    /**
     * Compare two list of {@link OmniboxSuggestion} to see if they are same.
     *
     * @return True if two list of {@link OmniboxSuggestion} are same, otherwise false.
     */
    boolean isOmniboxSuggestionListMatch(
            List<OmniboxSuggestion> list1, List<OmniboxSuggestion> list2) {
        if (list1.size() != list2.size()) return false;
        for (OmniboxSuggestion suggestion1 : list1) {
            for (OmniboxSuggestion suggestion2 : list2) {
                if (isCachedOmniboxSuggestionMatch(suggestion1, suggestion2)) {
                    list2.remove(suggestion2);
                    break;
                }
            }
        }
        return list2.isEmpty();
    }

    /**
     * Build a dummy suggestions list .
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<OmniboxSuggestion> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            OmniboxSuggestion suggestion = new OmniboxSuggestion(
                    OmniboxSuggestionType.CLIPBOARD_IMAGE, false /* isSearchType */,
                    0 /* relevance */, 0 /* transition */, "dummy text 1" + (index + 1),
                    null /* displayTextClassifications */,
                    "dummy description 1" + (index + 1) /* description */,
                    null /* descriptionClassifications */, null /* answer */,
                    null /* fillIntoEdit */, new GURL("dummy url" + (index + 1)) /* url */,
                    GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                    false /* isStarred */, false /* isDeletable */,
                    hasPostData ? "Dummy Content Type" + (index + 1) : null /* postContentType */,
                    hasPostData ? new byte[] {4, 5, 6, (byte) (index + 1)} : null /* postData */,
                    OmniboxSuggestion.INVALID_GROUP);
            list.add(suggestion);
        }

        return list;
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithPostdataBeforeAndAfterAreSame() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, true);
        OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(list1);
        List<OmniboxSuggestion> list2 =
                OmniboxSuggestion.getCachedOmniboxSuggestionsForZeroSuggest();
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithoutPostdataBeforeAndAfterAreSame() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, false);
        OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(list1);
        List<OmniboxSuggestion> list2 =
                OmniboxSuggestion.getCachedOmniboxSuggestionsForZeroSuggest();
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }

    @CalledByNativeJavaTest
    public void groupHeaders_restoreHeadersFromEmptyCache() {
        SparseArray<String> cachedHeaders =
                OmniboxSuggestion.getCachedOmniboxSuggestionHeadersForZeroSuggest();
        Assert.assertNull(cachedHeaders);
    }

    @CalledByNativeJavaTest
    public void groupHeaders_cacheAllSaneGroupHeaders() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(10, "Header For Group 10");
        headers.put(20, "Header For Group 20");
        headers.put(30, "Header For Group 30");

        OmniboxSuggestion.cacheOmniboxSuggestionHeadersForZeroSuggest(headers);
        SparseArray<String> cachedHeaders =
                OmniboxSuggestion.getCachedOmniboxSuggestionHeadersForZeroSuggest();

        Assert.assertEquals(headers.size(), cachedHeaders.size());
        for (int index = 0; index < headers.size(); index++) {
            Assert.assertEquals(headers.keyAt(index), cachedHeaders.keyAt(index));
            Assert.assertEquals(headers.valueAt(index), cachedHeaders.valueAt(index));
        }
    }

    @CalledByNativeJavaTest
    public void groupHeaders_cachePartiallySaneGroupHeadersDropsInvalidEntries() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(10, "Header For Group 10");
        headers.put(OmniboxSuggestion.INVALID_GROUP, "Header For Group 20");
        headers.put(30, null);

        OmniboxSuggestion.cacheOmniboxSuggestionHeadersForZeroSuggest(headers);
        SparseArray<String> cachedHeaders =
                OmniboxSuggestion.getCachedOmniboxSuggestionHeadersForZeroSuggest();

        Assert.assertEquals(1, cachedHeaders.size());
        Assert.assertEquals(10, cachedHeaders.keyAt(0));
        Assert.assertEquals("Header For Group 10", cachedHeaders.valueAt(0));
    }

    @CalledByNativeJavaTest
    public void groupHeaders_restoreInvalidHeadersFromCache() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(OmniboxSuggestion.INVALID_GROUP, "This group is invalid");
        headers.put(20, "");
        headers.put(30, null);

        OmniboxSuggestion.cacheOmniboxSuggestionHeadersForZeroSuggest(headers);
        SparseArray<String> cachedHeaders =
                OmniboxSuggestion.getCachedOmniboxSuggestionHeadersForZeroSuggest();
        Assert.assertEquals(0, cachedHeaders.size());
    }

    @CalledByNativeJavaTest
    public void dropSuggestions_suggestionsWithNoGroups() {
        List<OmniboxSuggestion> list = buildDummySuggestionsList(2, false);
        SparseArray<String> headers = new SparseArray<>();
        OmniboxSuggestion.dropSuggestionsWithIncorrectGroupHeaders(list, headers);
        Assert.assertEquals(2, list.size());
    }

    @CalledByNativeJavaTest
    public void dropSuggestions_suggestionsWithValidGroupsAssociation() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, false);
        List<OmniboxSuggestion> list2 = buildDummySuggestionsList(2, false);
        OmniboxSuggestion suggestionWithHeader = new OmniboxSuggestion(
                OmniboxSuggestionType.SEARCH_SUGGEST, true /* isSearchType */, 0 /* relevance */,
                0 /* transition */, "dummy text", null /* displayTextClassifications */,
                "dummy description" /* description */, null /* descriptionClassifications */,
                null /* answer */, null /* fillIntoEdit */, new GURL("dummy url") /* url */,
                GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                false /* isStarred */, false /* isDeletable */, null /* postContentType */,
                null /* postData */, 1);
        list1.add(suggestionWithHeader);
        list2.add(suggestionWithHeader);

        SparseArray<String> headers = new SparseArray<>();
        headers.put(1, "Valid Header");

        OmniboxSuggestion.dropSuggestionsWithIncorrectGroupHeaders(list2, headers);
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }

    @CalledByNativeJavaTest
    public void dropSuggestions_suggestionsWithInvalidGroupsAssociation() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, false);
        List<OmniboxSuggestion> list2 = buildDummySuggestionsList(2, false);
        OmniboxSuggestion suggestionWithHeader = new OmniboxSuggestion(
                OmniboxSuggestionType.SEARCH_SUGGEST, true /* isSearchType */, 0 /* relevance */,
                0 /* transition */, "dummy text", null /* displayTextClassifications */,
                "dummy description" /* description */, null /* descriptionClassifications */,
                null /* answer */, null /* fillIntoEdit */, new GURL("dummy url") /* url */,
                GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                false /* isStarred */, false /* isDeletable */, null /* postContentType */,
                null /* postData */, 1);
        list2.add(suggestionWithHeader);

        OmniboxSuggestion.dropSuggestionsWithIncorrectGroupHeaders(list2, new SparseArray<>());
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }
}
