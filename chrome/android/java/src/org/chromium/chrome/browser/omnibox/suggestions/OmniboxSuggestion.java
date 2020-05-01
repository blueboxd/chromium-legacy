// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.text.TextUtils;
import android.util.Base64;
import android.util.SparseArray;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Container class with information about each omnibox suggestion item.
 */
@VisibleForTesting
public class OmniboxSuggestion {
    public static final int INVALID_GROUP = -1;

    private static final String KEY_ZERO_SUGGEST_LIST_SIZE = "zero_suggest_list_size";
    private static final String KEY_PREFIX_ZERO_SUGGEST_URL = "zero_suggest_url";
    private static final String KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST = "zero_suggest_display_text";
    private static final String KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION = "zero_suggest_description";
    private static final String KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE = "zero_suggest_native_type";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE = "zero_suggest_is_search";
    private static final String KEY_PREFIX_ZERO_SUGGEST_ANSWER_TEXT = "zero_suggest_answer_text";
    private static final String KEY_PREFIX_ZERO_SUGGEST_GROUP_ID = "zero_suggest_group_id";
    private static final String KEY_ZERO_SUGGEST_HEADER_LIST_SIZE = "zero_suggest_header_list_size";
    private static final String KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_ID =
            "zero_suggest_header_group_id";
    private static final String KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_TITLE =
            "zero_suggest_header_group_title";
    // Deprecated:
    // private static final String KEY_PREFIX_ZERO_SUGGEST_ANSWER_TYPE = "zero_suggest_answer_type";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE = "zero_suggest_is_deletable";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_STARRED = "zero_suggest_is_starred";
    private static final String KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_TYPE =
            "zero_suggest_post_content_type";
    private static final String KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_DATA =
            "zero_suggest_post_content_data";

    /**
     * Specifies the style of portions of the suggestion text.
     * <p>
     * ACMatchClassification (as defined in C++) further describes the fields and usage.
     */
    public static class MatchClassification {
        /**
         * The offset into the text where this classification begins.
         */
        public final int offset;

        /**
         * A bitfield that determines the style of this classification.
         * @see MatchClassificationStyle
         */
        public final int style;

        public MatchClassification(int offset, int style) {
            this.offset = offset;
            this.style = style;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof MatchClassification)) return false;
            MatchClassification other = (MatchClassification) obj;
            return offset == other.offset && style == other.style;
        }
    }

    private final int mType;
    private final boolean mIsSearchType;
    private final String mDisplayText;
    private final List<MatchClassification> mDisplayTextClassifications;
    private final String mDescription;
    private final List<MatchClassification> mDescriptionClassifications;
    private final SuggestionAnswer mAnswer;
    private final String mFillIntoEdit;
    private final GURL mUrl;
    private final GURL mImageUrl;
    private final String mImageDominantColor;
    private final int mRelevance;
    private final int mTransition;
    private final boolean mIsStarred;
    private final boolean mIsDeletable;
    private final String mPostContentType;
    private final byte[] mPostData;
    private final int mGroupId;

    public OmniboxSuggestion(int nativeType, boolean isSearchType, int relevance, int transition,
            String displayText, List<MatchClassification> displayTextClassifications,
            String description, List<MatchClassification> descriptionClassifications,
            SuggestionAnswer answer, String fillIntoEdit, GURL url, GURL imageUrl,
            String imageDominantColor, boolean isStarred, boolean isDeletable,
            String postContentType, byte[] postData, int groupId) {
        mType = nativeType;
        mIsSearchType = isSearchType;
        mRelevance = relevance;
        mTransition = transition;
        mDisplayText = displayText;
        mDisplayTextClassifications = displayTextClassifications;
        mDescription = description;
        mDescriptionClassifications = descriptionClassifications;
        mAnswer = answer;
        mFillIntoEdit = TextUtils.isEmpty(fillIntoEdit) ? displayText : fillIntoEdit;
        assert url != null;
        mUrl = url;
        assert imageUrl != null;
        mImageUrl = imageUrl;
        mImageDominantColor = imageDominantColor;
        mIsStarred = isStarred;
        mIsDeletable = isDeletable;
        mPostContentType = postContentType;
        mPostData = postData;
        mGroupId = groupId;
    }

    public int getType() {
        return mType;
    }

    public int getTransition() {
        return mTransition;
    }

    public String getDisplayText() {
        return mDisplayText;
    }

    public List<MatchClassification> getDisplayTextClassifications() {
        return mDisplayTextClassifications;
    }

    public String getDescription() {
        return mDescription;
    }

    public List<MatchClassification> getDescriptionClassifications() {
        return mDescriptionClassifications;
    }

    public SuggestionAnswer getAnswer() {
        return mAnswer;
    }

    public boolean hasAnswer() {
        return mAnswer != null;
    }

    public String getFillIntoEdit() {
        return mFillIntoEdit;
    }

    public GURL getUrl() {
        return mUrl;
    }

    public GURL getImageUrl() {
        return mImageUrl;
    }

    @Nullable
    public String getImageDominantColor() {
        return mImageDominantColor;
    }

    /**
     * @return Whether the suggestion is a search suggestion.
     */
    public boolean isSearchSuggestion() {
        return mIsSearchType;
    }

    /**
     * @return Whether this suggestion represents a starred/bookmarked URL.
     */
    public boolean isStarred() {
        return mIsStarred;
    }

    public boolean isDeletable() {
        return mIsDeletable;
    }

    public String getPostContentType() {
        return mPostContentType;
    }

    public byte[] getPostData() {
        return mPostData;
    }

    /**
     * @return The relevance score of this suggestion.
     */
    public int getRelevance() {
        return mRelevance;
    }

    @Override
    public String toString() {
        return mType + " relevance=" + mRelevance + " \"" + mDisplayText + "\" -> " + mUrl;
    }

    @Override
    public int hashCode() {
        int hash = 37 * mType + mDisplayText.hashCode() + mFillIntoEdit.hashCode()
                + (mIsStarred ? 1 : 0) + (mIsDeletable ? 1 : 0);
        if (mAnswer != null) hash = hash + mAnswer.hashCode();
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof OmniboxSuggestion)) {
            return false;
        }

        OmniboxSuggestion suggestion = (OmniboxSuggestion) obj;
        return mType == suggestion.mType
                && TextUtils.equals(mFillIntoEdit, suggestion.mFillIntoEdit)
                && TextUtils.equals(mDisplayText, suggestion.mDisplayText)
                && ObjectsCompat.equals(
                        mDisplayTextClassifications, suggestion.mDisplayTextClassifications)
                && TextUtils.equals(mDescription, suggestion.mDescription)
                && ObjectsCompat.equals(
                        mDescriptionClassifications, suggestion.mDescriptionClassifications)
                && mIsStarred == suggestion.mIsStarred && mIsDeletable == suggestion.mIsDeletable
                && ObjectsCompat.equals(mAnswer, suggestion.mAnswer);
    }

    /**
     * Cache the given suggestion list in shared preferences.
     * @param suggestions Suggestions to be cached.
     */
    public static void cacheOmniboxSuggestionListForZeroSuggest(
            List<OmniboxSuggestion> suggestions) {
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putInt(KEY_ZERO_SUGGEST_LIST_SIZE, suggestions.size()).apply();
        for (int i = 0; i < suggestions.size(); i++) {
            OmniboxSuggestion suggestion = suggestions.get(i);
            if (suggestion.mAnswer != null) continue;

            editor.putString(KEY_PREFIX_ZERO_SUGGEST_URL + i, suggestion.getUrl().serialize())
                    .putString(
                            KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST + i, suggestion.getDisplayText())
                    .putString(KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION + i, suggestion.getDescription())
                    .putInt(KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE + i, suggestion.getType())
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE + i,
                            suggestion.isSearchSuggestion())
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE + i, suggestion.isDeletable())
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_STARRED + i, suggestion.isStarred())
                    .putString(KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_TYPE + i,
                            suggestion.getPostContentType())
                    .putString(KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_DATA + i,
                            suggestion.getPostData() == null
                                    ? ""
                                    : Base64.encodeToString(
                                            suggestion.getPostData(), Base64.DEFAULT))
                    .putInt(KEY_PREFIX_ZERO_SUGGEST_GROUP_ID + i, suggestion.getGroupId())
                    .apply();
        }
    }

    /**
     * @return The zero suggest result if they have been cached before.
     */
    public static List<OmniboxSuggestion> getCachedOmniboxSuggestionsForZeroSuggest() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int size = prefs.getInt(KEY_ZERO_SUGGEST_LIST_SIZE, -1);
        List<OmniboxSuggestion> suggestions = null;
        if (size > 1) {
            suggestions = new ArrayList<>(size);
            List<MatchClassification> classifications = new ArrayList<>();
            classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
            for (int i = 0; i < size; i++) {
                // TODO(tedchoc): Answers in suggest were previously cached, but that could lead to
                //                stale or misleading answers for cases like weather.  Ignore any
                //                previously cached answers for several releases while any previous
                //                results are cycled through.
                String answerText = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_ANSWER_TEXT + i, "");
                if (!TextUtils.isEmpty(answerText)) continue;

                GURL url = GURL.deserialize(prefs.getString(KEY_PREFIX_ZERO_SUGGEST_URL + i, ""));
                if (url == null) continue;
                String displayText = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST + i, "");
                String description = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION + i, "");
                int nativeType = prefs.getInt(KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE + i, -1);
                boolean isSearchType =
                        prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE + i, true);
                boolean isStarred = prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_STARRED + i, false);
                boolean isDeletable =
                        prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE + i, false);
                String postContentType =
                        prefs.getString(KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_TYPE + i, "");
                byte[] postData = Base64.decode(
                        prefs.getString(KEY_PREFIX_ZERO_SUGGEST_POST_CONTENT_DATA + i, ""),
                        Base64.DEFAULT);
                int groupId = prefs.getInt(KEY_PREFIX_ZERO_SUGGEST_GROUP_ID + i, INVALID_GROUP);

                OmniboxSuggestion suggestion = new OmniboxSuggestion(nativeType, isSearchType, 0, 0,
                        displayText, classifications, description, classifications, null, "", url,
                        GURL.emptyGURL(), null, isStarred, isDeletable,
                        postContentType.isEmpty() ? null : postContentType,
                        postData.length == 0 ? null : postData, groupId);
                suggestions.add(suggestion);
            }
        }
        return suggestions;
    }

    /**
     * @return The Group headers for zero suggest results if they have been cached before.
     */
    public static SparseArray<String> getCachedOmniboxSuggestionHeadersForZeroSuggest() {
        final SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        final int size = prefs.getInt(KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, -1);

        if (size <= 0) return null;

        final SparseArray<String> headers = new SparseArray<>(size);
        for (int i = 0; i < size; i++) {
            int groupId = prefs.getInt(KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_ID + i, INVALID_GROUP);
            String groupTitle =
                    prefs.getString(KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_TITLE + i, null);
            if (groupId == INVALID_GROUP || TextUtils.isEmpty(groupTitle)) continue;
            headers.put(groupId, groupTitle);
        }
        return headers;
    }

    /**
     * Cache the given suggestion group headers map in shared preferences.
     * @param groupHeaders Group headers to cache.
     */
    public static void cacheOmniboxSuggestionHeadersForZeroSuggest(SparseArray<String> headers) {
        final Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putInt(KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, headers.size()).apply();

        for (int i = 0; i < headers.size(); i++) {
            editor.putInt(KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_ID + i, headers.keyAt(i))
                    .putString(KEY_PREFIX_ZERO_SUGGEST_HEADER_GROUP_TITLE + i, headers.valueAt(i))
                    .apply();
        }
    }

    /**
     * Iterates through suggestions list and removes all suggestions that report association with
     * nonexistent group headers.
     *
     * @param suggestions List of suggestions to check.
     * @param groupHeaders Group headers to check suggestions against.
     */
    public static void dropSuggestionsWithIncorrectGroupHeaders(
            List<OmniboxSuggestion> suggestions, SparseArray<String> groupHeaders) {
        if (suggestions == null || suggestions.isEmpty()) return;

        for (int index = suggestions.size() - 1; index >= 0; index--) {
            final int groupId = suggestions.get(index).getGroupId();
            if (groupId != INVALID_GROUP && groupHeaders.indexOfKey(groupId) < 0) {
                suggestions.remove(index);
            }
        }
    }

    /**
     * @return ID of the group this suggestion is associated with, or null, if the suggestion is
     *         not associated with any group, or INVALID_GROUP if suggestion is not associated with
     *         any group.
     */
    public int getGroupId() {
        return mGroupId;
    }
}
