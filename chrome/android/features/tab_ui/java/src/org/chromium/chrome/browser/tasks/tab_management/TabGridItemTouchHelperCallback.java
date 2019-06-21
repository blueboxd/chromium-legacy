// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.util.List;

/**
 * A {@link ItemTouchHelper.SimpleCallback} implementation to host the logic for swipe and drag
 * related actions in grid related layouts.
 */
public class TabGridItemTouchHelperCallback extends ItemTouchHelper.SimpleCallback {

    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabListMediator.TabActionListener mTabClosedListener;
    private final String mComponentName;
    private final TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    private float mSwipeToDismissThreshold;
    private float mMergeThreshold;
    private float mUngroupThreshold;
    private boolean mActionsOnAllRelatedTabs;
    private int mDragFlags;
    private int mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
    private int mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
    private int mUnGroupTabIndex = TabModel.INVALID_TAB_INDEX;
    private RecyclerView mRecyclerView;

    public TabGridItemTouchHelperCallback(TabListModel tabListModel,
            TabModelSelector tabModelSelector, TabListMediator.TabActionListener tabClosedListener,
            TabListMediator.TabGridDialogHandler tabGridDialogHandler, String componentName,
            boolean actionsOnAllRelatedTabs) {
        super(0, 0);
        mModel = tabListModel;
        mTabModelSelector = tabModelSelector;
        mTabClosedListener = tabClosedListener;
        mComponentName = componentName;
        mActionsOnAllRelatedTabs = actionsOnAllRelatedTabs;
        mTabGridDialogHandler = tabGridDialogHandler;
    }

    /**
     * This method sets up parameters that are used by the {@link ItemTouchHelper} to make decisions
     * about user actions.
     * @param swipeToDismissThreshold          Defines the threshold that user needs to swipe in
     *         order to be considered as a remove operation.
     * @param mergeThreshold                   Defines the threshold of how much two items need to
     *         be overlapped in order to be considered as a merge operation.
     */
    void setupCallback(
            float swipeToDismissThreshold, float mergeThreshold, float ungroupThreshold) {
        mSwipeToDismissThreshold = swipeToDismissThreshold;
        mMergeThreshold = mergeThreshold;
        mUngroupThreshold = ungroupThreshold;
        boolean isTabGroupEnabled = FeatureUtilities.isTabGroupsAndroidEnabled();
        boolean isTabGroupUiImprovementEnabled =
                FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled();
        // Only enable drag for users with group disabled, or with group and group ui improvement
        // enabled at the same time.
        boolean isDragEnabled = !isTabGroupEnabled || isTabGroupUiImprovementEnabled;
        mDragFlags = isDragEnabled ? ItemTouchHelper.START | ItemTouchHelper.END
                        | ItemTouchHelper.UP | ItemTouchHelper.DOWN
                                   : 0;
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        final int dragFlags = mDragFlags;
        final int swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
        mRecyclerView = recyclerView;
        return makeMovementFlags(dragFlags, swipeFlags);
    }

    @Override
    public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder fromViewHolder,
            RecyclerView.ViewHolder toViewHolder) {
        assert fromViewHolder instanceof TabGridViewHolder;
        assert toViewHolder instanceof TabGridViewHolder;

        mSelectedTabIndex = toViewHolder.getAdapterPosition();
        if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX) {
            mModel.updateHoveredTabForMergeToGroup(mHoveredTabIndex, false);
            mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
        }

        int currentTabId = ((TabGridViewHolder) fromViewHolder).getTabId();
        int destinationTabId = ((TabGridViewHolder) toViewHolder).getTabId();
        int distance = toViewHolder.getAdapterPosition() - fromViewHolder.getAdapterPosition();
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (filter instanceof EmptyTabModelFilter) {
            tabModel.moveTab(currentTabId,
                    mModel.indexFromId(currentTabId) + (distance > 0 ? distance + 1 : distance));
        } else if (!mActionsOnAllRelatedTabs) {
            int destinationIndex = tabModel.indexOf(mTabModelSelector.getTabById(destinationTabId));
            tabModel.moveTab(currentTabId, distance > 0 ? destinationIndex + 1 : destinationIndex);
        } else {
            List<Tab> destinationTabGroup = getRelatedTabsForId(destinationTabId);
            int newIndex = distance >= 0
                    ? TabGroupUtils.getLastTabModelIndexForList(tabModel, destinationTabGroup) + 1
                    : TabGroupUtils.getFirstTabModelIndexForList(tabModel, destinationTabGroup);
            ((TabGroupModelFilter) filter).moveRelatedTabs(currentTabId, newIndex);
        }
        RecordUserAction.record("TabGrid.Drag.Reordered." + mComponentName);
        return true;
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int i) {
        assert viewHolder instanceof TabGridViewHolder;

        mTabClosedListener.run(((TabGridViewHolder) viewHolder).getTabId());
        RecordUserAction.record("MobileStackViewSwipeCloseTab." + mComponentName);
    }

    @Override
    public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            mSelectedTabIndex = viewHolder.getAdapterPosition();
            mModel.updateSelectedTabForMergeToGroup(mSelectedTabIndex, true);
            RecordUserAction.record("TabGrid.Drag.Start." + mComponentName);
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
            if (!FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()) {
                mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
            }
            if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX && mActionsOnAllRelatedTabs) {
                onTabMergeToGroup(mSelectedTabIndex, mHoveredTabIndex);
                mRecyclerView.removeViewAt(mSelectedTabIndex);
                RecordUserAction.record("GridTabSwitcher.Drag.AddToGroupOrCreateGroup");
            }
            mModel.updateSelectedTabForMergeToGroup(mSelectedTabIndex, false);
            if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX) {
                mModel.updateHoveredTabForMergeToGroup(mSelectedTabIndex > mHoveredTabIndex
                                ? mHoveredTabIndex
                                : mHoveredTabIndex - 1,
                        false);
            }
            if (mUnGroupTabIndex != TabModel.INVALID_TAB_INDEX) {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                filter.moveTabOutOfGroup(mModel.get(mUnGroupTabIndex).get(TabProperties.TAB_ID));
                mRecyclerView.removeViewAt(mUnGroupTabIndex);
                RecordUserAction.record("TabGridDialog.Drag.RemoveFromGroup");
            }
            mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
            mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
            mUnGroupTabIndex = TabModel.INVALID_TAB_INDEX;
            if (mTabGridDialogHandler != null) {
                mTabGridDialogHandler.updateUngroupBarStatus(
                        TabGridDialogParent.UngroupBarStatus.HIDE);
            }
        }
    }

    @Override
    public void onChildDraw(Canvas c, RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder,
            float dX, float dY, int actionState, boolean isCurrentlyActive) {
        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
        if (actionState == ItemTouchHelper.ACTION_STATE_SWIPE) {
            float alpha = Math.max(0.2f, 1f - 0.8f * Math.abs(dX) / mSwipeToDismissThreshold);
            int index = mModel.indexFromId(((TabGridViewHolder) viewHolder).getTabId());
            if (index == TabModel.INVALID_TAB_INDEX) return;

            mModel.get(index).set(TabProperties.ALPHA, alpha);
        } else if (actionState == ItemTouchHelper.ACTION_STATE_DRAG && mActionsOnAllRelatedTabs) {
            if (!FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()) return;
            int prev_hovered = mHoveredTabIndex;
            mHoveredTabIndex = TabListRecyclerView.getHoveredTabIndex(
                    recyclerView, viewHolder.itemView, dX, dY, mMergeThreshold);
            mModel.updateHoveredTabForMergeToGroup(mHoveredTabIndex, true);
            if (prev_hovered != mHoveredTabIndex) {
                mModel.updateHoveredTabForMergeToGroup(prev_hovered, false);
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_DRAG
                && mTabGridDialogHandler != null) {
            // Not allow ungrouping the last tab in group.
            if (recyclerView.getAdapter().getItemCount() == 1) return;
            boolean isHoveredOnUngroupBar = viewHolder.itemView.getBottom() + dY
                    > recyclerView.getBottom() - mUngroupThreshold;
            mUnGroupTabIndex = isHoveredOnUngroupBar ? viewHolder.getAdapterPosition()
                                                     : TabModel.INVALID_TAB_INDEX;
            mTabGridDialogHandler.updateUngroupBarStatus(isHoveredOnUngroupBar
                            ? TabGridDialogParent.UngroupBarStatus.HOVERED
                            : (mSelectedTabIndex == TabModel.INVALID_TAB_INDEX
                                            ? TabGridDialogParent.UngroupBarStatus.HIDE
                                            : TabGridDialogParent.UngroupBarStatus.SHOW));
        }
    }

    @Override
    public float getSwipeThreshold(RecyclerView.ViewHolder viewHolder) {
        return mSwipeToDismissThreshold / viewHolder.itemView.getWidth();
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    private void onTabMergeToGroup(int selectedCardIndex, int hoveredCardIndex) {
        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        filter.mergeTabsToGroup(filter.getTabAt(selectedCardIndex).getId(),
                filter.getTabAt(hoveredCardIndex).getId());
    }

    @VisibleForTesting
    void setActionsOnAllRelatedTabsForTest(boolean flag) {
        mActionsOnAllRelatedTabs = flag;
    }

    @VisibleForTesting
    void setHoveredTabIndexForTest(int index) {
        mHoveredTabIndex = index;
    }

    @VisibleForTesting
    void setSelectedTabIndexForTest(int index) {
        mSelectedTabIndex = index;
    }

    @VisibleForTesting
    void setUnGroupTabIndexForTest(int index) {
        mUnGroupTabIndex = index;
    }
}
