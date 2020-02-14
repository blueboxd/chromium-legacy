// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.RECYCLER_VIEW_TAG;

import android.content.Context;
import android.support.v7.widget.OrientationHelper;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.EditDistance;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

import java.util.List;

/**
 * A coordinator responsible for suggesting chips to the user. If there is one chip to display, it
 * will be centered on the screen. If there are more than one chip, all but the last one will be
 * displayed from the right of the screen to the left, and the last one will be displayed at the
 * left of the screen.
 *
 * <p>For instance, if chips = [1, 2, 3]:
 * |              |
 * |[3]     [2][1]|
 * |              |
 *
 * <p>If there are too many chips to display all of them on the screen, the carousel will scroll
 * such that the last chips is fixed. For instance, if chips = [1, 2, 3, 4, 5, 6]:
 * |               |
 * |[6][4][3][2][1]|       (before)
 * |               |
 *
 * |               |
 * |[6][5][4][3][2]|       (after horizontal scrolling from left to right)
 * |               |
 */
public class AssistantActionsCarouselCoordinator {
    private final AssistantCarouselModel mModel;
    private final RecyclerView mView;

    public AssistantActionsCarouselCoordinator(Context context, AssistantCarouselModel model) {
        mModel = model;

        mView = new RecyclerView(context);
        mView.setTag(RECYCLER_VIEW_TAG);

        CustomLayoutManager layoutManager = new CustomLayoutManager();
        // Workaround for b/128679161.
        layoutManager.setMeasurementCacheEnabled(false);
        mView.setLayoutManager(layoutManager);
        mView.addItemDecoration(new AssistantActionsDecoration(context, layoutManager));

        // TODO(crbug.com/806868): WRAP_CONTENT height should work instead of setting the exact
        // height of the view. We add the sheet vertical spacing twice as the item decoration will
        // add this space above and below each chip, and remove the vertical inset added to all
        // ButtonView's.
        mView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                context.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size)
                        + 2
                                * context.getResources().getDimensionPixelSize(
                                        R.dimen.autofill_assistant_bottombar_vertical_spacing)
                        - 2
                                * context.getResources().getDimensionPixelSize(
                                        R.dimen.autofill_assistant_button_bg_vertical_inset)));

        mView.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model.getChipsModel(),
                        AssistantChipViewHolder::getViewType, AssistantChipViewHolder::bind),
                AssistantChipViewHolder::create));
    }

    public RecyclerView getView() {
        return mView;
    }

    public void updateChips(List<AssistantChip> newChips) {
        ListModel<AssistantChip> oldChips = mModel.getChipsModel();
        if (!areChipsEqual(oldChips, newChips)) {
            // We apply the minimum set of operations on the current chips to transform it in the
            // target list of chips. When testing for chip equivalence, we only compare their type
            // and text but all substitutions will still be applied so we are sure we display the
            // given {@code chips} with their associated callbacks.
            EditDistance.transform(oldChips, newChips, AssistantChip::equals);
        } else {
            for (int i = 0; i < oldChips.size(); ++i) {
                AssistantChip oldChip = oldChips.get(i);
                AssistantChip newChip = newChips.get(i);

                // We assume that the enabled state is the only thing that may change.
                if (oldChip.isDisabled() == newChip.isDisabled()) {
                    continue;
                }

                View view = mView.getLayoutManager().findViewByPosition(i);
                if (view == null) {
                    oldChips.update(i, newChip);
                } else {
                    oldChip.setDisabled(newChip.isDisabled());
                    view.setEnabled(!newChip.isDisabled());
                }
            }
        }
    }

    /** Changes the visibility of all chips not matching the identifier **/
    public void setAllChipsVisibleExcept(String identifier, boolean visible) {
        ListModel<AssistantChip> chips = mModel.getChipsModel();
        for (int i = 0; i < chips.size(); ++i) {
            View view = mView.getLayoutManager().findViewByPosition(i);
            if (view != null && !chips.get(i).getIdentifier().equals(identifier)) {
                view.setVisibility(visible ? View.VISIBLE : View.GONE);
            }
        }
    }

    /**
     * Returns |true| if all chips in the list are considered equal, meaning the same except their
     * |isDisabled()| state.
     */
    private boolean areChipsEqual(ListModel<AssistantChip> oldChips, List<AssistantChip> newChips) {
        if (oldChips.size() != newChips.size()) {
            return false;
        }

        for (int i = 0; i < oldChips.size(); ++i) {
            AssistantChip oldChip = oldChips.get(i);
            AssistantChip newChip = newChips.get(i);

            if (!oldChip.equals(newChip)) {
                return false;
            }
        }

        return true;
    }

    // TODO(crbug.com/806868): Handle RTL layouts.
    // TODO(crbug.com/806868): Recycle invisible children instead of laying all of them out.
    static class CustomLayoutManager extends RecyclerView.LayoutManager {
        final OrientationHelper mOrientationHelper;

        private int mMaxScroll;
        private int mScroll;

        CustomLayoutManager() {
            mOrientationHelper = OrientationHelper.createHorizontalHelper(this);
        }

        @Override
        public RecyclerView.LayoutParams generateDefaultLayoutParams() {
            return new RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        @Override
        public boolean isAutoMeasureEnabled() {
            // We need to enable auto measure to support RecyclerView's that wrap their content
            // height.
            return true;
        }

        @Override
        public boolean canScrollHorizontally() {
            return mMaxScroll > 0;
        }

        @Override
        public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
            int itemCount = state.getItemCount();
            detachAndScrapAttachedViews(recycler);
            if (itemCount == 0) {
                return;
            }

            int extraWidth = getWidth() - getPaddingLeft() - getPaddingRight();

            // Add children and measure them. The extra available width will be added after the
            // first chip if there are more than one, or will be used to center it if there is only
            // one.
            for (int i = 0; i < itemCount; i++) {
                View child = recycler.getViewForPosition(i);
                addView(child);
                measureChildWithMargins(child, 0, 0);
                int childWidth = mOrientationHelper.getDecoratedMeasurement(child);
                extraWidth -= childWidth;
            }

            // If we are missing space, allow scrolling.
            if (extraWidth < 0) {
                mMaxScroll = -extraWidth;
            } else {
                mMaxScroll = 0;
            }
            mScroll = 0;

            int top = getPaddingTop();
            int right = getWidth() - getPaddingRight();

            // Layout all child views but the last one from right to left.
            for (int i = 0; i < getChildCount() - 1; i++) {
                View child = getChildAt(i);
                int width = mOrientationHelper.getDecoratedMeasurement(child);
                int height = mOrientationHelper.getDecoratedMeasurementInOther(child);
                int bottom = top + height;
                int left = right - width;
                layoutDecoratedWithMargins(child, left, top, right, bottom);

                right = left;
            }

            // Layout last child on the left. We need to layout this one after the others such that
            // it sits on top of them when scrolling.
            int left = getPaddingLeft();
            if (itemCount == 1) {
                // Center it using extra available space.
                left += extraWidth / 2;
            }

            View firstChild = getChildAt(getChildCount() - 1);
            right = left + mOrientationHelper.getDecoratedMeasurement(firstChild);
            int bottom = top + mOrientationHelper.getDecoratedMeasurementInOther(firstChild);
            layoutDecoratedWithMargins(firstChild, left, top, right, bottom);
        }

        @Override
        public int scrollHorizontallyBy(
                int dx, RecyclerView.Recycler recycler, RecyclerView.State state) {
            // dx > 0 == scroll from right to left.
            int scrollBy = dx > 0 ? Math.min(mScroll, dx) : Math.max(-(mMaxScroll - mScroll), dx);
            mScroll -= scrollBy;

            // Offset all children except the last one.
            for (int i = 0; i < getChildCount() - 1; i++) {
                getChildAt(i).offsetLeftAndRight(-scrollBy);
            }

            return scrollBy;
        }
    }
}
