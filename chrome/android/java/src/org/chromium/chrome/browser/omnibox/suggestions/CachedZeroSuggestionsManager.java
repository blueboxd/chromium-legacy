// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_GROUP_ID_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_GROUP_ID_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_STARRED_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_URL_PREFIX;

import android.text.TextUtils;
import android.util.Base64;
import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * CachedZeroSuggestionsManager manages caching and restoring zero suggestions.
 */
public class CachedZeroSuggestionsManager {
    /**
     * Save the content of the CachedZeroSuggestionsManager to SharedPreferences cache.
     */
    public static void saveToCache(AutocompleteResult resultToCache) {
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        cacheSuggestionList(manager, resultToCache.getSuggestionsList());
        cacheGroupHeaders(manager, resultToCache.getGroupHeaders());
    }

    /**
     * Read previously stored AutocompleteResult from cache.
     * @return AutocompleteResult populated with the content of the SharedPreferences cache.
     */
    static AutocompleteResult readFromCache() {
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        List<OmniboxSuggestion> suggestions =
                CachedZeroSuggestionsManager.readCachedSuggestionList(manager);
        SparseArray<String> groupHeaders =
                CachedZeroSuggestionsManager.readCachedGroupHeaders(manager);
        removeInvalidSuggestionsAndGroupHeaders(suggestions, groupHeaders);
        return new AutocompleteResult(suggestions, groupHeaders);
    }

    /**
     * Cache suggestion list in shared preferences.
     *
     * @param prefs Shared preferences manager.
     */
    private static void cacheSuggestionList(
            SharedPreferencesManager prefs, List<OmniboxSuggestion> suggestions) {
        int numCachableSuggestions = 0;

        // Write 0 here to avoid something wrong in the for loop, and the real size will be updated
        // after the for loop.
        prefs.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 0);
        for (int i = 0; i < suggestions.size(); i++) {
            OmniboxSuggestion suggestion = suggestions.get(i);
            if (!shouldCacheSuggestion(suggestion)) continue;

            prefs.writeString(KEY_ZERO_SUGGEST_URL_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getUrl().serialize());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getDisplayText());
            prefs.writeString(KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getDescription());
            prefs.writeInt(KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getType());
            prefs.writeBoolean(
                    KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.isSearchSuggestion());
            prefs.writeBoolean(
                    KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.isDeletable());
            prefs.writeBoolean(KEY_ZERO_SUGGEST_IS_STARRED_PREFIX.createKey(numCachableSuggestions),
                    suggestion.isStarred());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getPostContentType());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getPostData() == null
                            ? null
                            : Base64.encodeToString(suggestion.getPostData(), Base64.DEFAULT));
            prefs.writeInt(KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getGroupId());
            numCachableSuggestions++;
        }
        prefs.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, numCachableSuggestions);
    }

    /**
     * Restore suggestion list from shared preferences.
     *
     * @param prefs Shared preferences manager.
     * @return List of Omnibox suggestions previously cached in shared preferences.
     */
    @NonNull
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static List<OmniboxSuggestion> readCachedSuggestionList(SharedPreferencesManager prefs) {
        int size = prefs.readInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, -1);
        if (size <= 1) {
            // Ignore case where we only have a single item on the list - it's likely
            // 'what-you-typed' suggestion.
            size = 0;
        }

        List<OmniboxSuggestion> suggestions = new ArrayList<>(size);
        List<OmniboxSuggestion.MatchClassification> classifications = new ArrayList<>();
        classifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
        for (int i = 0; i < size; i++) {
            // TODO(tedchoc): Answers in suggest were previously cached, but that could lead to
            //                stale or misleading answers for cases like weather.  Ignore any
            //                previously cached answers for several releases while any previous
            //                results are cycled through.
            String answerText =
                    prefs.readString(KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX.createKey(i), null);
            if (!TextUtils.isEmpty(answerText)) continue;

            GURL url = GURL.deserialize(
                    prefs.readString(KEY_ZERO_SUGGEST_URL_PREFIX.createKey(i), null));
            String displayText =
                    prefs.readString(KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX.createKey(i), null);
            String description =
                    prefs.readString(KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX.createKey(i), null);
            int nativeType = prefs.readInt(KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX.createKey(i),
                    OmniboxSuggestion.INVALID_TYPE);
            boolean isSearchType =
                    prefs.readBoolean(KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX.createKey(i), false);
            boolean isStarred =
                    prefs.readBoolean(KEY_ZERO_SUGGEST_IS_STARRED_PREFIX.createKey(i), false);
            boolean isDeletable =
                    prefs.readBoolean(KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX.createKey(i), false);
            String postContentType =
                    prefs.readString(KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX.createKey(i), null);
            String postDataStr =
                    prefs.readString(KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX.createKey(i), null);
            byte[] postData =
                    postDataStr == null ? null : Base64.decode(postDataStr, Base64.DEFAULT);
            int groupId = prefs.readInt(
                    KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(i), OmniboxSuggestion.INVALID_GROUP);

            OmniboxSuggestion suggestion = new OmniboxSuggestion(nativeType, isSearchType, 0, 0,
                    displayText, classifications, description, classifications, null, null, url,
                    GURL.emptyGURL(), null, isStarred, isDeletable, postContentType, postData,
                    groupId, null, null);
            suggestions.add(suggestion);
        }

        return suggestions;
    }

    /**
     * Cache suggestion group headers in shared preferences.
     *
     * @param prefs Shared preferences manager.
     */
    private static void cacheGroupHeaders(
            SharedPreferencesManager prefs, SparseArray<String> groupHeaders) {
        final int size = groupHeaders.size();
        prefs.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, size);
        for (int i = 0; i < size; i++) {
            prefs.writeInt(
                    KEY_ZERO_SUGGEST_HEADER_GROUP_ID_PREFIX.createKey(i), groupHeaders.keyAt(i));
            prefs.writeString(KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX.createKey(i),
                    groupHeaders.valueAt(i));
        }
    }

    /**
     * Restore group headers from shared preferences.
     *
     * @param prefs Shared preferences manager.
     * @return Map of group ID to header text previously cached in shared preferences.
     */
    @NonNull
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static SparseArray<String> readCachedGroupHeaders(SharedPreferencesManager prefs) {
        final int size = prefs.readInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, 0);
        final SparseArray<String> groupHeaders = new SparseArray<>(size);

        for (int i = 0; i < size; i++) {
            int groupId = prefs.readInt(KEY_ZERO_SUGGEST_HEADER_GROUP_ID_PREFIX.createKey(i),
                    OmniboxSuggestion.INVALID_GROUP);
            String groupTitle =
                    prefs.readString(KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX.createKey(i), null);
            groupHeaders.put(groupId, groupTitle);
        }
        return groupHeaders;
    }

    /**
     * Remove all invalid entries for group headers and omnibox suggestions.
     *
     * @param suggestions List of suggestions to scan for invalid entries.
     * @param groupHeaders Group headers to scan for invalid entries.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void removeInvalidSuggestionsAndGroupHeaders(
            List<OmniboxSuggestion> suggestions, SparseArray<String> groupHeaders) {
        // Remove all group headers that have invalid index or title.
        for (int index = groupHeaders.size() - 1; index >= 0; index--) {
            if (groupHeaders.keyAt(index) == OmniboxSuggestion.INVALID_GROUP
                    || TextUtils.isEmpty(groupHeaders.valueAt(index))) {
                groupHeaders.removeAt(index);
            }
        }

        // Remove all suggestions with no valid URL or pointing to nonexistent groups.
        for (int index = suggestions.size() - 1; index >= 0; index--) {
            final OmniboxSuggestion suggestion = suggestions.get(index);
            final int groupId = suggestion.getGroupId();
            if (!suggestion.getUrl().isValid() || suggestion.getUrl().isEmpty()
                    || (groupId != OmniboxSuggestion.INVALID_GROUP
                            && groupHeaders.indexOfKey(groupId) < 0)) {
                suggestions.remove(index);
            }
        }
    }

    /**
     * Check if the suggestion is needed to be cached.
     *
     * @param suggestion The OmniboxSuggestion to check.
     * @return Whether or not the suggestion can be cached.
     */
    private static boolean shouldCacheSuggestion(OmniboxSuggestion suggestion) {
        return !suggestion.hasAnswer()
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_URL
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_TEXT
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }
}
