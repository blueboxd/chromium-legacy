// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.TabListModelProperties.MODEL_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.TabListModelProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.TabListModelProperties.ModelType.OTHERS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.TabListModelProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * A {@link PropertyListModel} implementation to keep information about a list of
 * {@link org.chromium.chrome.browser.tab.Tab}s.
 */
class TabListModel extends ModelList {
    /**
     * Required properties for each {@link PropertyModel} managed by this {@link ModelList}.
     */
    static class TabListModelProperties {
        /** Supported Model type within this ModelList. */
        @IntDef({TAB, MESSAGE, OTHERS})
        @Retention(RetentionPolicy.SOURCE)
        public @interface ModelType {
            int TAB = 0;
            int MESSAGE = 1;
            int OTHERS = 2;
        }

        public static final PropertyModel.ReadableIntPropertyKey MODEL_TYPE =
                new PropertyModel.ReadableIntPropertyKey();
    }

    /**
     * Convert the given tab ID to an index to match during partial updates.
     * @param tabId The tab ID to search for.
     * @return The index within the model {@link org.chromium.ui.modelutil.SimpleList}.
     */
    public int indexFromId(int tabId) {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(MODEL_TYPE) == TAB && model.get(TAB_ID) == tabId) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Get the index that matches a message item that has the given message type.
     * @param messageType The message type to match.
     * @return The index within the model.
     */
    public int lastIndexForMessageItemFromType(int messageType) {
        for (int i = size() - 1; i >= 0; i--) {
            PropertyModel model = get(i).model;
            if (model.get(MODEL_TYPE) == MESSAGE && model.get(MESSAGE_TYPE) == messageType) {
                return i;
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    @Override
    public void add(int position, MVCListAdapter.ListItem item) {
        assert validateListItem(item);
        super.add(position, item);
    }

    private boolean validateListItem(MVCListAdapter.ListItem item) {
        try {
            item.model.get(MODEL_TYPE);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }

    /**
     * Sync the {@link TabListModel} with updated information. Update tab id of
     * the item in {@code index} with the current selected {@code tab} of the group.
     * @param selectedTab   The current selected tab in the group.
     * @param index         The index of the item in {@link TabListModel} that needs to be updated.
     */
    void updateTabListModelIdForGroup(Tab selectedTab, int index) {
        get(index).model.set(TabProperties.TAB_ID, selectedTab.getId());
    }

    /**
     * This method gets indexes in the {@link TabListModel} of the two tabs that are merged into one
     * group.
     * @param tabModel   The tabModel that owns the tabs.
     * @param tabs       The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index of the tab that is selected to merge and
     * the second member as the index of the tab that is being merged into.
     */
    Pair<Integer, Integer> getIndexesForMergeToGroup(TabModel tabModel, List<Tab> tabs) {
        int desIndex = TabModel.INVALID_TAB_INDEX;
        int srcIndex = TabModel.INVALID_TAB_INDEX;
        int lastTabModelIndex = tabModel.indexOf(tabs.get(tabs.size() - 1));
        for (int i = lastTabModelIndex; i >= 0; i--) {
            Tab curTab = tabModel.getTabAt(i);
            if (!tabs.contains(curTab)) break;
            int index = indexFromId(curTab.getId());
            if (index != TabModel.INVALID_TAB_INDEX && srcIndex == TabModel.INVALID_TAB_INDEX) {
                srcIndex = index;
            } else if (index != TabModel.INVALID_TAB_INDEX
                    && desIndex == TabModel.INVALID_TAB_INDEX) {
                desIndex = index;
            }
        }
        return new Pair<>(desIndex, srcIndex);
    }

    /**
     * This method updates the information in {@link TabListModel} of the selected tab when a merge
     * related operation happens.
     * @param index         The index of the item in {@link TabListModel} that needs to be updated.
     * @param isSelected    Whether the tab is selected or not in a merge related operation. If
     *         selected, update the corresponding item in {@link TabListModel} to the selected
     *         state. If not, restore it to original state.
     */
    void updateSelectedTabForMergeToGroup(int index, boolean isSelected) {
        int status = isSelected ? ClosableTabGridView.AnimationStatus.SELECTED_CARD_ZOOM_IN
                                : ClosableTabGridView.AnimationStatus.SELECTED_CARD_ZOOM_OUT;
        if (index < 0 || index >= size()
                || get(index).model.get(TabProperties.CARD_ANIMATION_STATUS) == status) {
            return;
        }

        get(index).model.set(TabProperties.CARD_ANIMATION_STATUS, status);
        get(index).model.set(TabProperties.ALPHA, isSelected ? 0.8f : 1f);
    }

    /**
     * This method updates the information in {@link TabListModel} of the hovered tab when a merge
     * related operation happens.
     * @param index         The index of the item in {@link TabListModel} that needs to be updated.
     * @param isHovered     Whether the tab is hovered or not in a merge related operation. If
     *         hovered, update the corresponding item in {@link TabListModel} to the hovered state.
     *         If not, restore it to original state.
     */
    void updateHoveredTabForMergeToGroup(int index, boolean isHovered) {
        int status = isHovered ? ClosableTabGridView.AnimationStatus.HOVERED_CARD_ZOOM_IN
                               : ClosableTabGridView.AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        if (index < 0 || index >= size()
                || get(index).model.get(TabProperties.CARD_ANIMATION_STATUS) == status) {
            return;
        }

        get(index).model.set(TabProperties.CARD_ANIMATION_STATUS, status);
    }
}
