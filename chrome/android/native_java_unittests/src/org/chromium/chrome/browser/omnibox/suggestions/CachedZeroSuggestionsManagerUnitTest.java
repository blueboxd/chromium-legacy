// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link CachedZeroSuggestionsManager}.
 */
public class CachedZeroSuggestionsManagerUnitTest {
    @CalledByNative
    private CachedZeroSuggestionsManagerUnitTest() {}

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

        for (int index = 0; index < count; ++index) {
            final int id = index + 1;
            list.add(createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL)
                             .setPostContentType(hasPostData ? "Content Type " + id : null)
                             .setPostData(hasPostData ? new byte[] {4, 5, 6, (byte) id} : null)
                             .build());
        }

        return list;
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @return Newly constructed OmniboxSuggestion.
     */
    private OmniboxSuggestionBuilderForTest createSuggestionBuilder(int id) {
        return createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL);
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @param type Suggestion type.
     * @return Newly constructed OmniboxSuggestion.
     */
    private OmniboxSuggestionBuilderForTest createSuggestionBuilder(
            int id, @OmniboxSuggestionType int type) {
        return OmniboxSuggestionBuilderForTest.searchWithType(type)
                .setDisplayText("dummy text " + id)
                .setDescription("dummy description " + id);
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
    public void setNewSuggestions_DoNotcacheClipboardSuggestions() {
        List<OmniboxSuggestion> mix_list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.CLIPBOARD_IMAGE).build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                createSuggestionBuilder(3, OmniboxSuggestionType.CLIPBOARD_TEXT).build(),
                createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());
        List<OmniboxSuggestion> expected_list =
                Arrays.asList(createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                        createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());

        AutocompleteResult dataToCache = new AutocompleteResult(mix_list, null);
        AutocompleteResult dataToExpected = new AutocompleteResult(expected_list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToExpected, dataFromCache);
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
        List<OmniboxSuggestion> list = buildDummySuggestionsList(2, false);
        list.add(createSuggestionBuilder(33).setGroupId(1).build());
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
        listToCache.add(createSuggestionBuilder(33).setGroupId(1).build());

        AutocompleteResult dataExpected = new AutocompleteResult(listExpected, null);
        AutocompleteResult dataToCache = new AutocompleteResult(listToCache, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataExpected, dataFromCache);
    }

    @CalledByNativeJavaTest
    public void malformedCache_dropsMissingSuggestions() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

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
        rawDataFromCache.getSuggestionsList().add(new OmniboxSuggestionBuilderForTest().build());

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
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

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
        listExpected.add(createSuggestionBuilder(72).setGroupId(12).build());

        List<OmniboxSuggestion> listWithInvalidItems = buildDummySuggestionsList(2, false);
        listWithInvalidItems.add(createSuggestionBuilder(72).setGroupId(12).build());
        listWithInvalidItems.add(
                createSuggestionBuilder(73).setGroupId(12).setUrl(new GURL("bad URL")).build());
        listWithInvalidItems.add(createSuggestionBuilder(74).setGroupId(34).build());

        AutocompleteResult dataWithInvalidItems =
                new AutocompleteResult(listWithInvalidItems, headersWithInvalidItems);
        AutocompleteResult dataExpected = new AutocompleteResult(listExpected, headersExpected);

        CachedZeroSuggestionsManager.removeInvalidSuggestionsAndGroupHeaders(
                dataWithInvalidItems.getSuggestionsList(), dataWithInvalidItems.getGroupHeaders());
        assertAutocompleteResultEquals(dataWithInvalidItems, dataExpected);
    }
}
