// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.List;

/** Manages the list of DropdownItemViewInfo elements. */
class DropdownItemViewInfoListManager {
    private static final int OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS = 10;
    private final ModelList mManagedModel;
    private int mLayoutDirection;
    private @OmniboxTheme int mOmniboxTheme;
    private List<DropdownItemViewInfo> mSourceViewInfoList;
    private SparseArray<Boolean> mGroupVisibility;

    DropdownItemViewInfoListManager(@NonNull ModelList managedModel) {
        assert managedModel != null : "Must specify a non-null model.";
        mLayoutDirection = View.LAYOUT_DIRECTION_INHERIT;
        mOmniboxTheme = OmniboxTheme.LIGHT_THEME;
        mSourceViewInfoList = Collections.emptyList();
        mGroupVisibility = new SparseArray<>();
        mManagedModel = managedModel;
    }

    /** @return Total count of view infos that may be shown in the Omnibox Suggestions list. */
    int getSuggestionsCount() {
        return mSourceViewInfoList.size();
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        if (mLayoutDirection == layoutDirection) return;

        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
        }
    }

    /**
     * Specifies the visual theme to be used by the suggestions.
     * @param omniboxTheme Specifies which {@link OmniboxTheme} should be used.
     */
    void setOmniboxTheme(@OmniboxTheme int omniboxTheme) {
        if (mOmniboxTheme == omniboxTheme) return;

        mOmniboxTheme = omniboxTheme;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.OMNIBOX_THEME, omniboxTheme);
        }
    }

    /**
     * Toggle the visibility state of suggestions belonging to specific group.
     *
     * @param groupId ID of the group whose visibility state is expected to change.
     * @param state Visibility state of the suggestions belonging to the group.
     */
    void setGroupVisibility(int groupId, boolean state) {
        if (getGroupVisibility(groupId) == state) return;
        mGroupVisibility.put(groupId, state);
        if (!state) {
            removeSuggestionsForGroup(groupId);
        } else {
            insertSuggestionsForGroup(groupId);
        }
    }

    /**
     * @param groupId ID of the suggestions group.
     * @return True, if group should be visible, otherwise false.
     */
    private boolean getGroupVisibility(int groupId) {
        return mGroupVisibility.get(groupId, /* defaultVisibility= */ true);
    }

    /** @return Whether the supplied view info is a header for the specific group of suggestions. */
    private boolean isGroupHeaderWithId(DropdownItemViewInfo info, int groupId) {
        return (info.type == OmniboxSuggestionUiType.HEADER && info.groupId == groupId);
    }

    /** Clear all DropdownItemViewInfo lists. */
    void clear() {
        mSourceViewInfoList.clear();
        mManagedModel.clear();
        mGroupVisibility.clear();
    }

    /** Record histograms for all currently presented suggestions. */
    void recordSuggestionsShown() {
        int richEntitiesCount = 0;
        for (int index = 0; index < mManagedModel.size(); index++) {
            DropdownItemViewInfo info = (DropdownItemViewInfo) mManagedModel.get(index);
            info.processor.recordItemPresented(info.model);

            if (info.type == OmniboxSuggestionUiType.ENTITY_SUGGESTION) {
                richEntitiesCount++;
            }
        }

        // Note: valid range for histograms must start with (at least) 1. This does not prevent us
        // from reporting 0 as a count though - values lower than 'min' fall in the 'underflow'
        // bucket, while values larger than 'max' will be reported in 'overflow' bucket.
        RecordHistogram.recordLinearCountHistogram("Omnibox.RichEntityShown", richEntitiesCount, 1,
                OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS, OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS + 1);
    }

    /**
     * Specify the input list of DropdownItemViewInfo elements.
     *
     * @param sourceList Source list of ViewInfo elements.
     */
    void setSourceViewInfoList(@NonNull List<DropdownItemViewInfo> sourceList) {
        mSourceViewInfoList = sourceList;
        mGroupVisibility.clear();

        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.OMNIBOX_THEME, mOmniboxTheme);
        }
        // Note: Despite DropdownItemViewInfo extending ListItem, we can't use the
        // List<DropdownItemViewInfo> as a parameter expecting a List<ListItem>.
        // Java disallows casting one generic type to another, unless the specialization
        // is dropped.
        mManagedModel.set((List) mSourceViewInfoList);
    }

    /**
     * Remove all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void removeSuggestionsForGroup(int groupId) {
        int index;
        int count = 0;

        for (index = mManagedModel.size() - 1; index >= 0; index--) {
            DropdownItemViewInfo viewInfo = (DropdownItemViewInfo) mManagedModel.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                break;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }
        if (count > 0) {
            // Skip group header when dropping items.
            mManagedModel.removeRange(index + 1, count);
        }
    }

    /**
     * Insert all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void insertSuggestionsForGroup(int groupId) {
        int insertPosition = 0;

        // Search for the insert position.
        // Iterate through all *available* view infos until we find the first element that we
        // should insert. To determine the insertion point we skip past all *displayed* view
        // infos that were also preceding elements that we want to insert.
        for (; insertPosition < mManagedModel.size(); insertPosition++) {
            final DropdownItemViewInfo viewInfo =
                    (DropdownItemViewInfo) mManagedModel.get(insertPosition);
            // Insert suggestions directly below their header.
            if (isGroupHeaderWithId(viewInfo, groupId)) break;
        }

        // Check if reached the end of the list.
        if (insertPosition == mManagedModel.size()) return;

        // insertPosition points to header - advance the index and see if we already have
        // elements belonging to that group on the list.
        insertPosition++;
        if (insertPosition < mManagedModel.size()
                && ((DropdownItemViewInfo) mManagedModel.get(insertPosition)).groupId == groupId) {
            return;
        }

        // Find elements to insert.
        int firstElementIndex = -1;
        int count = 0;
        for (int index = 0; index < mSourceViewInfoList.size(); index++) {
            final DropdownItemViewInfo viewInfo = mSourceViewInfoList.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                firstElementIndex = index + 1;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }

        if (count != 0 && firstElementIndex != -1) {
            mManagedModel.addAll(
                    mSourceViewInfoList.subList(firstElementIndex, firstElementIndex + count),
                    insertPosition);
        }
    }
}
