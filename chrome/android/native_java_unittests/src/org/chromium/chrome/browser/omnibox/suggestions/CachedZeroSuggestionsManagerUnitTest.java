// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link CachedZeroSuggestionsManager}.
 */
public class CachedZeroSuggestionsManagerUnitTest {
    @CalledByNative
    private CachedZeroSuggestionsManagerUnitTest() {}

    @CalledByNative
    void setUp() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
    }

    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        final List<OmniboxSuggestion> list1 = data1.getSuggestionsList();
        final List<OmniboxSuggestion> list2 = data2.getSuggestionsList();
        Assert.assertEquals(list1, list2);

        final SparseArray<String> headers1 = data1.getGroupHeaders();
        final SparseArray<String> headers2 = data2.getGroupHeaders();
        Assert.assertEquals(headers1.size(), headers2.size());
        for (int index = 0; index < headers1.size(); index++) {
            Assert.assertEquals(headers1.keyAt(index), headers2.keyAt(index));
            Assert.assertEquals(headers1.valueAt(index), headers2.valueAt(index));
        }
    }

    /**
     * Build a dummy suggestions list.
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<OmniboxSuggestion> list = new ArrayList<>();

        List<OmniboxSuggestion.MatchClassification> dummyClassifications = new ArrayList<>();
        dummyClassifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));

        for (int index = 0; index < count; ++index) {
            OmniboxSuggestion suggestion = new OmniboxSuggestion(
                    OmniboxSuggestionType.CLIPBOARD_IMAGE, false /* isSearchType */,
                    0 /* relevance */, 0 /* transition */, "dummy text 1" + (index + 1),
                    dummyClassifications /* displayTextClassifications */,
                    "dummy description 1" + (index + 1) /* description */,
                    dummyClassifications /* descriptionClassifications */, null /* answer */,
                    null /* fillIntoEdit */, new GURL("http://dummy.com/" + (index + 1)) /* url */,
                    GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                    false /* isStarred */, false /* isDeletable */,
                    hasPostData ? "Dummy Content Type" + (index + 1) : null /* postContentType */,
                    hasPostData ? new byte[] {4, 5, 6, (byte) (index + 1)} : null /* postData */,
                    OmniboxSuggestion.INVALID_GROUP, null /* queryTiles */);
            list.add(suggestion);
        }

        return list;
    }

    private OmniboxSuggestion buildUninitializedSuggestion() {
        List<OmniboxSuggestion.MatchClassification> classifications = new ArrayList<>();
        classifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));

        return new OmniboxSuggestion(-1, true /* isSearchType */, 0 /* relevance */,
                0 /* transition */, "" /* displayText */,
                classifications /* displayTextClassifications */, "" /* description */,
                classifications /* descriptionClassifications */, null /* answer */,
                "" /* fillIntoEdit */, GURL.emptyGURL() /* url */, GURL.emptyGURL() /* imageUrl */,
                null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */,
                null /* postContentType */, null /* postData */, OmniboxSuggestion.INVALID_GROUP,
                null /* queryTiles */);
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(2, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithoutPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(2, false), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void groupHeaders_restoreHeadersFromEmptyCache() {
        // Note: purge cache explicitly, because tests are run on an actual device
        // and cache may hold content from other test runs.
        AutocompleteResult dataToCache = new AutocompleteResult(null, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void groupHeaders_cacheAllSaneGroupHeaders() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(10, "Header For Group 10");
        headers.put(20, "Header For Group 20");
        headers.put(30, "Header For Group 30");
        AutocompleteResult dataToCache = new AutocompleteResult(null, headers);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void groupHeaders_cachePartiallySaneGroupHeadersDropsInvalidEntries() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(10, "Header For Group 10");
        headers.put(OmniboxSuggestion.INVALID_GROUP, "Header For Group 20");
        headers.put(30, null);

        AutocompleteResult dataToCache = new AutocompleteResult(null, headers);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();

        SparseArray<String> validHeaders = new SparseArray<>();
        validHeaders.put(10, "Header For Group 10");
        assertAutocompleteResultEquals(dataFromCache, new AutocompleteResult(null, validHeaders));
    }

    @CalledByNativeJavaTest
    public void groupHeaders_restoreInvalidHeadersFromCache() {
        SparseArray<String> headers = new SparseArray<>();
        headers.put(OmniboxSuggestion.INVALID_GROUP, "This group is invalid");
        headers.put(20, "");
        headers.put(30, null);

        AutocompleteResult dataToCache = new AutocompleteResult(null, headers);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, new AutocompleteResult(null, null));
    }

    @CalledByNativeJavaTest
    public void dropSuggestions_suggestionsWithValidGroupsAssociation() {
        List<OmniboxSuggestion.MatchClassification> dummyClassifications = new ArrayList<>();
        dummyClassifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));

        List<OmniboxSuggestion> list = buildDummySuggestionsList(2, false);
        OmniboxSuggestion suggestionWithHeader =
                new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST, true /* isSearchType */,
                        0 /* relevance */, 0 /* transition */, "dummy text",
                        dummyClassifications /* displayTextClassifications */,
                        "dummy description" /* description */,
                        dummyClassifications /* descriptionClassifications */, null /* answer */,
                        null /* fillIntoEdit */, new GURL("http://dummy.url") /* url */,
                        GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                        false /* isStarred */, false /* isDeletable */, null /* postContentType */,
                        null /* postData */, 1 /* groupId */, null /* queryTiles */);
        list.add(suggestionWithHeader);
        SparseArray<String> headers = new SparseArray<>();
        headers.put(1, "Valid Header");

        AutocompleteResult dataToCache = new AutocompleteResult(list, headers);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void dropSuggestions_suggestionsWithInvalidGroupsAssociation() {
        List<OmniboxSuggestion> listExpected = buildDummySuggestionsList(2, false);
        List<OmniboxSuggestion> listToCache = buildDummySuggestionsList(2, false);
        OmniboxSuggestion suggestionWithHeader = new OmniboxSuggestion(
                OmniboxSuggestionType.SEARCH_SUGGEST, true /* isSearchType */, 0 /* relevance */,
                0 /* transition */, "dummy text", null /* displayTextClassifications */,
                "dummy description" /* description */, null /* descriptionClassifications */,
                null /* answer */, null /* fillIntoEdit */, new GURL("dummy url") /* url */,
                GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                false /* isStarred */, false /* isDeletable */, null /* postContentType */,
                null /* postData */, 1 /* groupId */, null /* queryTiles */);
        listToCache.add(suggestionWithHeader);

        AutocompleteResult dataExpected = new AutocompleteResult(listExpected, null);
        AutocompleteResult dataToCache = new AutocompleteResult(listToCache, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataExpected, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void malformedCache_dropsMissingSuggestions() {
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Save one valid suggestion to cache.
        AutocompleteResult dataToCache =
                new AutocompleteResult(buildDummySuggestionsList(1, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Signal that there's actually 2 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 2);

        // Construct an expected raw suggestion list content. This constitutes one valid entry
        // and 1 totally empty entry.
        AutocompleteResult rawDataFromCache =
                new AutocompleteResult(buildDummySuggestionsList(1, true), null);
        rawDataFromCache.getSuggestionsList().add(buildUninitializedSuggestion());

        // readCachedSuggestionList makes full attempt to restore whatever could be scraped from the
        // cache.
        List<OmniboxSuggestion> readList =
                CachedZeroSuggestionsManager.readCachedSuggestionList(manager);
        Assert.assertEquals(2, readList.size());
        assertAutocompleteResultEquals(new AutocompleteResult(readList, null), rawDataFromCache);

        // Cache recovery however should be smart here and remove items that make no sense.
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, dataToCache);
    }

    @CalledByNativeJavaTest
    public void malformedCache_dropsMissingGroupHeaders() {
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Write 3 wrong group headers to the cache
        SparseArray<String> headers = new SparseArray<>();
        headers.put(12, "Valid group");
        headers.put(34, "");
        headers.put(OmniboxSuggestion.INVALID_GROUP, "Invalid group");
        AutocompleteResult invalidDataToCache = new AutocompleteResult(null, headers);
        CachedZeroSuggestionsManager.saveToCache(invalidDataToCache);

        // Report that we actually have 4 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, 4);

        // Read raw suggestions from the cache. Note that the sparse array will only have 3 elements
        // because we put one item with INVALID_GROUP, and another such element will be deduced from
        // missing data.
        SparseArray<String> rawGroupHeaders =
                CachedZeroSuggestionsManager.readCachedGroupHeaders(manager);
        Assert.assertEquals(3, rawGroupHeaders.size());
        Assert.assertEquals(rawGroupHeaders.get(12), "Valid group");
        Assert.assertEquals(rawGroupHeaders.get(34), "");
        // missing group entry will overwrite our invalid group definition
        Assert.assertNull(rawGroupHeaders.get(OmniboxSuggestion.INVALID_GROUP));

        // Cache recovery however should be smart here and remove items that make no sense.
        SparseArray<String> wantHeaders = new SparseArray<>();
        wantHeaders.put(12, "Valid group");
        AutocompleteResult wantDataFromCache = new AutocompleteResult(null, wantHeaders);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();

        assertAutocompleteResultEquals(dataFromCache, wantDataFromCache);
    }

    @CalledByNativeJavaTest
    public void removeInvalidSuggestions_dropsInvalidSuggestionsAndHeaders() {
        // Write 3 wrong group headers to the cache
        SparseArray<String> headersExpected = new SparseArray<>();
        headersExpected.put(12, "Valid group");

        SparseArray<String> headersWithInvalidItems = new SparseArray<>();
        headersWithInvalidItems.put(12, "Valid group");
        headersWithInvalidItems.put(34, "");
        headersWithInvalidItems.put(OmniboxSuggestion.INVALID_GROUP, "Invalid group");

        List<OmniboxSuggestion> listExpected = buildDummySuggestionsList(2, false);
        listExpected.add(new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                true /* isSearchType */, 0 /* relevance */, 0 /* transition */, "dummy text",
                null /* displayTextClassifications */, "dummy description" /* description */,
                null /* descriptionClassifications */, null /* answer */, null /* fillIntoEdit */,
                new GURL("http://url.com") /* url */, GURL.emptyGURL() /* imageUrl */,
                null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */,
                null /* postContentType */, null /* postData */, 12 /* groupId */,
                null /* queryTiles */));

        List<OmniboxSuggestion> listWithInvalidItems = buildDummySuggestionsList(2, false);
        listWithInvalidItems.add(new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                true /* isSearchType */, 0 /* relevance */, 0 /* transition */, "dummy text",
                null /* displayTextClassifications */, "dummy description" /* description */,
                null /* descriptionClassifications */, null /* answer */, null /* fillIntoEdit */,
                new GURL("http://url.com") /* url */, GURL.emptyGURL() /* imageUrl */,
                null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */,
                null /* postContentType */, null /* postData */, 12 /* groupId */,
                null /* queryTiles */));
        listWithInvalidItems.add(new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                true /* isSearchType */, 0 /* relevance */, 0 /* transition */, "dummy text",
                null /* displayTextClassifications */, "dummy description" /* description */,
                null /* descriptionClassifications */, null /* answer */, null /* fillIntoEdit */,
                new GURL("bad url") /* url */, GURL.emptyGURL() /* imageUrl */,
                null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */,
                null /* postContentType */, null /* postData */, 12 /* groupId */,
                null /* queryTiles */));
        listWithInvalidItems.add(new OmniboxSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST,
                true /* isSearchType */, 0 /* relevance */, 0 /* transition */, "dummy text",
                null /* displayTextClassifications */, "dummy description" /* description */,
                null /* descriptionClassifications */, null /* answer */, null /* fillIntoEdit */,
                new GURL("http://url.com") /* url */, GURL.emptyGURL() /* imageUrl */,
                null /* imageDominantColor */, false /* isStarred */, false /* isDeletable */,
                null /* postContentType */, null /* postData */, 34 /* bad group */,
                null /* queryTiles */));

        AutocompleteResult dataWithInvalidItems =
                new AutocompleteResult(listWithInvalidItems, headersWithInvalidItems);
        AutocompleteResult dataExpected = new AutocompleteResult(listExpected, headersExpected);

        CachedZeroSuggestionsManager.removeInvalidSuggestionsAndGroupHeaders(
                dataWithInvalidItems.getSuggestionsList(), dataWithInvalidItems.getGroupHeaders());
        assertAutocompleteResultEquals(dataWithInvalidItems, dataExpected);
    }
}
