// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ITEM_COUNT_KEY;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Custom layout manager for site suggestions carousel. Handles custom effects such as
 * layout such that items at the edges are smaller than the center item and keeping focus
 * to the middle of the view.
 */
class SiteSuggestionsLayoutManager extends RecyclerView.LayoutManager {
    private static final float[] SCALE_FACTORS = {0.8f, 0.9f, 1f, 0.9f, 0.8f};
    private static final float SUM_FACTORS;
    // Initialize SUM_FACTORS to be sum of scale factors.
    static {
        float sum = 0f;
        for (float num : SCALE_FACTORS) {
            sum += num;
        }
        SUM_FACTORS = sum;
    }
    private static final int MAX_TILES = 5;

    private int mFocusPosition;
    private View mFocusView;
    private boolean mShouldFocusAfterLayout;
    private boolean mNeedRebind;
    private Context mContext;
    private RecyclerView mRecyclerView;
    private PropertyModel mModel;

    SiteSuggestionsLayoutManager(Context context, PropertyModel model) {
        setAutoMeasureEnabled(true);
        mContext = context;
        mFocusPosition = 0;
        mNeedRebind = false;
        mFocusView = null;
        mShouldFocusAfterLayout = false;
        mModel = model;
    }

    @Override
    public boolean onRequestChildFocus(
            RecyclerView parent, RecyclerView.State state, View child, View focused) {
        if (focused != null) {
            int newChildPos = getPosition(focused);

            // Stop scrolling if we're in the middle of a swipe/fling, see https://crbug.com/982357.
            parent.stopScroll();

            scrollToPosition(newChildPos);
        }
        return true;
    }

    @Override
    public boolean supportsPredictiveItemAnimations() {
        return false;
    }

    @Override
    public RecyclerView.LayoutParams generateDefaultLayoutParams() {
        return new RecyclerView.LayoutParams(
                RecyclerView.LayoutParams.WRAP_CONTENT, RecyclerView.LayoutParams.WRAP_CONTENT);
    }

    /**
     * Calculates what views should be shown in the viewport and lays them out. Inherently does not
     * support animations.
     *
     * @param recycler Recycler to use for fetching potentially cached views for a position.
     * @param state Transient state of the RecyclerView.
     */
    @Override
    public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
        if (getItemCount() == 0) {
            return;
        }
        int currentFirstPosition = 0;
        if (getChildCount() != 0) {
            currentFirstPosition = getPosition(getChildAt(0));
        }

        SparseArray<View> viewCache = new SparseArray<>();
        if (mNeedRebind) {
            // If we need to re-bind all the data, recycle everything to force re-bind.
            removeAndRecycleAllViews(recycler);
            mNeedRebind = false;
        } else {
            // Temporarily cache all views.
            for (int i = 0; i < getChildCount(); i++) {
                viewCache.append(currentFirstPosition + i, getChildAt(i));
            }

            // Temporarily detach all views.
            for (int i = 0; i < viewCache.size(); i++) {
                detachView(viewCache.valueAt(i));
            }
        }

        int futureFirstPosition = findFirstVisibleItemPosition();
        int futureLastPosition = findLastVisibleItemPosition();

        // Adds all the views necessary.
        for (int i = futureFirstPosition; i <= futureLastPosition; i++) {
            View child = viewCache.get(i);
            if (child == null) {
                // If only 1 view, set spacers around the view such that it appears in the center.
                child = recycler.getViewForPosition(i);
                addView(child);
            } else {
                viewCache.remove(i);
                attachView(child);
            }
            if (i == mFocusPosition) {
                mFocusView = child;
            }
        }

        resizeViews();

        // Recycle all unused views.
        for (int i = 0; i < viewCache.size(); i++) {
            removeDetachedView(viewCache.valueAt(i));
            recycler.recycleView(viewCache.valueAt(i));
        }
    }

    @Override
    public void onLayoutCompleted(RecyclerView.State state) {
        super.onLayoutCompleted(state);
        if (mShouldFocusAfterLayout) {
            mShouldFocusAfterLayout = false;
            mFocusView.requestFocus();
        }
    }

    @Override
    public boolean canScrollHorizontally() {
        return getItemCount() > 1;
    }

    /**
     * Scrolls the views by a delta. Layout new views only when one has been completely
     * scrolled off the screen.
     *
     * Note that this method does not scale the children as they move across the screen and does not
     * layout partial views, which may be a suboptimal visual experience on a normal device.
     *
     * TODO(chili): Soften the jump when the new tile appears.
     */
    @Override
    public int scrollHorizontallyBy(
            int dx, RecyclerView.Recycler recycler, RecyclerView.State state) {
        if (getItemCount() <= 1 || getChildCount() == 0) return 0;

        // Move all children by -dx. This is because if we're dragging the elements to the left
        // (<0), scrollbar is moving to the right (>0).
        offsetChildrenHorizontal(-dx);

        // Whether the first item is completely visible.
        boolean isFirstItemVisible = getDecoratedRight(getChildAt(0)) > 0;
        // Whether the last item is completely visible.
        boolean isLastItemVisible = getDecoratedLeft(getChildAt(getChildCount() - 1)) < getWidth();

        if (!isFirstItemVisible || !isLastItemVisible) {
            if (dx > 0) {
                mFocusPosition++;
            } else {
                mFocusPosition--;
            }
            onLayoutChildren(recycler, state);
        }

        return dx;
    }

    @Override
    public void scrollToPosition(int position) {
        if (position == mFocusPosition) return;
        mFocusPosition = position;
        assertNotInLayoutOrScroll("Scroll called within scroll");
        requestLayout();
    }

    /**
     * Returns what the first visible item position should be. Might not be accurate during layout.
     */
    public int findFirstVisibleItemPosition() {
        return Math.max(0, mFocusPosition - 2);
    }

    /**
     * Returns what the last visible item position should be. Might not be accurate during layout.
     */
    public int findLastVisibleItemPosition() {
        int lastVisiblePosition = mFocusPosition + 2;
        if (lastVisiblePosition < 0 || lastVisiblePosition == Integer.MAX_VALUE) {
            lastVisiblePosition = Integer.MAX_VALUE - 1;
        }
        if (lastVisiblePosition >= getItemCount()) {
            lastVisiblePosition = getItemCount() - 1;
        }
        return lastVisiblePosition;
    }

    @Override
    public void onItemsChanged(RecyclerView recyclerView) {
        requestFullRebind();
    }

    @Override
    public void onItemsAdded(RecyclerView recyclerView, int positionStart, int itemCount) {
        requestFullRebind();
    }

    @Override
    public void onItemsRemoved(RecyclerView recyclerView, int positionStart, int itemCount) {
        requestFullRebind();
    }

    @Override
    public void onItemsUpdated(RecyclerView recyclerView, int positionStart, int itemCount) {
        requestFullRebind();
    }

    @Override
    public void onItemsMoved(RecyclerView recyclerView, int from, int to, int itemCount) {
        requestFullRebind();
    }

    @Override
    public void onAttachedToWindow(RecyclerView view) {
        super.onAttachedToWindow(view);
        mRecyclerView = view;
    }

    @Override
    public void onDetachedFromWindow(RecyclerView view, RecyclerView.Recycler recycler) {
        super.onDetachedFromWindow(view, recycler);
        mRecyclerView = null;
    }

    @Override
    public int getRowCountForAccessibility(
            RecyclerView.Recycler recycler, RecyclerView.State state) {
        return 1;
    }

    @Override
    public int getColumnCountForAccessibility(
            RecyclerView.Recycler recycler, RecyclerView.State state) {
        return mModel.get(ITEM_COUNT_KEY);
    }

    /**
     * Requests layout such that it forces rebind of all visible items with
     * updated data from the adapter.
     */
    private void requestFullRebind() {
        mNeedRebind = true;
        requestLayout();
    }

    void focusCenterItem() {
        focusCenterItem(View.FOCUS_DOWN, null);
    }

    boolean focusCenterItem(int direction, Rect previouslyFocusedRect) {
        if (mRecyclerView == null || mRecyclerView.isComputingLayout() || mFocusView == null) {
            mShouldFocusAfterLayout = true;
            return true;
        }
        return mFocusView.requestFocus(direction, previouslyFocusedRect);
    }

    /**
     * Resizes currently attached views based on their position (larger in the middle).
     *
     * Views are centered, with increasing space between them as the screen widens. We assume
     * that the screens are at least wide enough to display all of the icons at with minimal space.
     * The tiles will overlap if there is a screen too small to do so.
     */
    private void resizeViews() {
        int leftOffset = 0;
        final int defaultIconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.most_likely_tile_size);
        // Amount of space to leave between items such that it would take up all available width.
        final int horizontalSpacer =
                (int) ((getWidth() - SUM_FACTORS * defaultIconSize) / (MAX_TILES - 1));

        // Number of "empty" spaces to leave on the left side.
        // This helps ensure that mFocusPosition is laid out in the center even if there are
        // no elements to the left of it.
        int leftSpaceCount =
                (SCALE_FACTORS.length / 2) - mFocusPosition + findFirstVisibleItemPosition();
        int leftDecrement = leftSpaceCount;

        for (int i = 0; i < Math.min(getChildCount() + leftSpaceCount, MAX_TILES); i++) {
            int iconSize = (int) (defaultIconSize * SCALE_FACTORS[i]);
            int decoratedChildWidth = iconSize;
            if (leftDecrement == 0) {
                View child = getChildAt(i - leftSpaceCount);

                // Scale according to position.
                ViewGroup.LayoutParams params = child.getLayoutParams();

                params.height = iconSize;
                params.width = iconSize;
                child.setLayoutParams(params);

                // Offset the top so that it's centered vertically.
                int topOffset = (defaultIconSize - iconSize) / 2;

                measureChildWithMargins(child, 0, 0);
                decoratedChildWidth = getDecoratedMeasuredWidth(child);
                int decoratedChildHeight = getDecoratedMeasuredHeight(child);

                layoutDecoratedWithMargins(child, leftOffset, topOffset,
                        leftOffset + decoratedChildWidth, topOffset + decoratedChildHeight);
            } else {
                leftDecrement--;
            }

            // The next tile needs to be <horizontal spacer> padded away from this tile.
            leftOffset += decoratedChildWidth + horizontalSpacer;
        }
    }
}
