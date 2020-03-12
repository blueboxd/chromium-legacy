// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

/** LinearLayout with special measuring code for GridRow */
public class GridRowView extends LinearLayout {
    private static final String TAG = "GridRowView";
    private final Supplier<Boolean> mIsRtlSupplier;

    // Total width of the GridRowView's internal padding, child widths, and child margins.
    // This will not equal getMeasuredWidth() when the GridRowView is MATCH_PARENT width and the
    // parent is wider than the space required by the GridRowView.
    // This is the analogue of LinearLayout.mTotalLength
    @VisibleForTesting
    int mTotalContentWidth;

    // Copied from LinearLayout.
    private int mViewGravity = Gravity.START | Gravity.TOP;

    // TODO: call to setOrientation(int) not allowed on the given receiver.
    @SuppressWarnings("nullness:method.invocation.invalid")
    public GridRowView(Context context, Supplier<Boolean> isRtlSupplier) {
        super(context);
        this.mIsRtlSupplier = isRtlSupplier;
        super.setOrientation(HORIZONTAL);
    }

    @Override
    public void setOrientation(int orientation) {
        if (orientation != HORIZONTAL) {
            throw new IllegalArgumentException("GridRowView can only be horizontal");
        }
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f, false);
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams lp) {
        if (lp instanceof LayoutParams) {
            return new LayoutParams((LayoutParams) lp);
        } else if (lp instanceof LinearLayout.LayoutParams) {
            return new LayoutParams((LinearLayout.LayoutParams) lp);
        }
        return new LayoutParams(lp);
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
        return p instanceof LayoutParams;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                || MeasureSpec.getSize(widthMeasureSpec) == 0) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        // Here's how much space we actually have; the GridRowView will fill it.
        // (This will not be 0/UNSPECIFIED because that is handled above)
        int maxWidth = MeasureSpec.getSize(widthMeasureSpec);

        int gridRowVerticalPadding = getPaddingTop() + getPaddingBottom();
        int gridRowHorizontalPadding = getPaddingLeft() + getPaddingRight();

        // Max height for child cells; either AT_MOST or UNSPECIFIED
        int maxHeightSpec;
        int heightMode = MeasureSpec.getMode(heightMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        if (heightMode == MeasureSpec.UNSPECIFIED) {
            maxHeightSpec = heightMeasureSpec;
        } else {
            maxHeightSpec = MeasureSpec.makeMeasureSpec(
                    heightSize - gridRowVerticalPadding, MeasureSpec.AT_MOST);
        }

        int unlimitedSpace = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        // See how much space the fixed-size (WRAP_CONTENT and width) cells take.
        // Ask how much space they would fill if there were no constraints.
        // Also add up the total weight for distribution among the weight cells later.
        int totalReservedWidth = 0;
        float totalWeight = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (view.getVisibility() == View.GONE) {
                continue;
            }
            LayoutParams params = ((LayoutParams) view.getLayoutParams());
            if (params.weight == 0
                    && (params.width > 0 || params.width == LayoutParams.WRAP_CONTENT)
                    && !params.mIsCollapsible) {
                if (params.width == LayoutParams.WRAP_CONTENT) {
                    view.measure(unlimitedSpace, getMaxHeightSpecForView(params, maxHeightSpec));
                } else {
                    view.measure(MeasureSpec.makeMeasureSpec(params.width, MeasureSpec.EXACTLY),
                            getMaxHeightSpecForView(params, maxHeightSpec));
                }
                totalReservedWidth += view.getMeasuredWidth();
            } else if (params.weight > 0) {
                totalWeight += params.weight;
            }
            totalReservedWidth += MarginLayoutParamsCompat.getMarginStart(params)
                    + MarginLayoutParamsCompat.getMarginEnd(params);
        }
        totalReservedWidth += gridRowHorizontalPadding;

        // Calculate width remaining to the collapsible cell(s)
        int availableWidth = Math.max(maxWidth - totalReservedWidth, 0);

        // Find collapsible cells and assign them the available width.
        // The first cell will get as much width as it needs (or as much as is available), and
        // future cells will get whatever's left.
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (view.getVisibility() == View.GONE) {
                continue;
            }

            LayoutParams params = ((LayoutParams) view.getLayoutParams());
            if (!params.mIsCollapsible) {
                continue;
            }
            int viewMaxHeight = getMaxHeightSpecForView(params, maxHeightSpec);
            // TODO: Handle margins on collapsible cells
            int desiredWidth;
            if (availableWidth == 0) {
                // Shortcut for when we run out of room.
                desiredWidth = 0;
            } else if (params.width == LayoutParams.WRAP_CONTENT) {
                view.measure(unlimitedSpace, viewMaxHeight);
                desiredWidth = view.getMeasuredWidth();
            } else {
                desiredWidth = params.width;
            }
            view.measure(MeasureSpec.makeMeasureSpec(
                                 Math.min(availableWidth, desiredWidth), MeasureSpec.EXACTLY),
                    viewMaxHeight);

            totalReservedWidth += view.getMeasuredWidth();
            availableWidth = Math.max(0, availableWidth - view.getMeasuredWidth());
        }

        // Distribute remaining width among the weight cells.
        if (totalWeight > 0) {
            for (int i = 0; i < getChildCount(); i++) {
                View view = getChildAt(i);
                if (view.getVisibility() == View.GONE) {
                    continue;
                }
                LayoutParams params = ((LayoutParams) view.getLayoutParams());
                if (params.weight > 0) {
                    view.measure(MeasureSpec.makeMeasureSpec(
                                         (int) (availableWidth * params.weight / totalWeight),
                                         MeasureSpec.EXACTLY),
                            getMaxHeightSpecForView(params, maxHeightSpec));
                }
            }
        }

        // Truncate remaining cells if we ran out of room.
        availableWidth = maxWidth - gridRowHorizontalPadding;
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (view.getVisibility() == View.GONE) {
                continue;
            }
            LayoutParams params = ((LayoutParams) view.getLayoutParams());
            availableWidth -= MarginLayoutParamsCompat.getMarginStart(params);
            if (view.getMeasuredWidth() > availableWidth) {
                view.measure(MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.EXACTLY),
                        maxHeightSpec);
            }
            availableWidth = Math.max(0,
                    availableWidth - view.getMeasuredWidth()
                            - MarginLayoutParamsCompat.getMarginEnd(params));
        }

        // Set height of the GridRowView based on the max cell height
        // Divided by 2 so we can create a MeasureSpec out of it (needs some free bits).
        int rowHeight = Integer.MAX_VALUE / 2;
        if (heightMode == MeasureSpec.EXACTLY) {
            rowHeight = heightSize;
        } else {
            if (heightMode == MeasureSpec.AT_MOST) {
                rowHeight = MeasureSpec.getSize(heightMeasureSpec);
            }
            int maxCellHeight = 0;
            for (int i = 0; i < getChildCount(); i++) {
                View view = getChildAt(i);
                if (view.getVisibility() == View.GONE) {
                    continue;
                }
                LayoutParams params = ((LayoutParams) view.getLayoutParams());
                int cellHeight = view.getMeasuredHeight() + params.topMargin + params.bottomMargin;
                maxCellHeight = Math.max(cellHeight, maxCellHeight);
            }
            // We should not need to resize the cells because they were all already smaller than
            // this.
            rowHeight = Math.min(rowHeight,
                    Math.max(maxCellHeight + gridRowVerticalPadding, getMinimumHeight()));
        }

        // Resize any cells that are MATCH_PARENT or have a min height on them.
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (view.getVisibility() == View.GONE) {
                continue;
            }
            LayoutParams params = ((LayoutParams) view.getLayoutParams());
            if (params.height == LayoutParams.MATCH_PARENT) {
                view.measure(
                        MeasureSpec.makeMeasureSpec(view.getMeasuredWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(rowHeight, MeasureSpec.EXACTLY));
            } else if (params.height == LayoutParams.WRAP_CONTENT
                    && view.getMinimumHeight() > view.getMeasuredHeight()
                    && view.getMinimumHeight() < rowHeight) {
                view.measure(
                        MeasureSpec.makeMeasureSpec(view.getMeasuredWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(view.getMinimumHeight(), MeasureSpec.EXACTLY));
            }
        }

        // Handle WRAP_CONTENT on the GridRowView - width of the row is the sum of the cell widths.
        // This does not make sense when there are weight cells, since they take up all available
        // space.
        if (getLayoutParams() != null && getLayoutParams().width == LayoutParams.WRAP_CONTENT
                && totalWeight == 0) {
            maxWidth = Math.min(maxWidth, totalReservedWidth);
        }

        // Calculate totalContentWidth
        mTotalContentWidth = gridRowHorizontalPadding;
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (view.getVisibility() == View.GONE) {
                continue;
            }
            mTotalContentWidth += view.getMeasuredWidth();
            LayoutParams params = ((LayoutParams) view.getLayoutParams());
            mTotalContentWidth += MarginLayoutParamsCompat.getMarginStart(params)
                    + MarginLayoutParamsCompat.getMarginEnd(params);
        }

        setMeasuredDimension(maxWidth, rowHeight);
    }

    /**
     * Find either the height specified by LayoutParams, or the height constrained by the parent.
     */
    private int getMaxHeightSpecForView(LayoutParams viewLayoutParams, int parentMaxHeightSpec) {
        int marginHeight = viewLayoutParams.topMargin + viewLayoutParams.bottomMargin;
        if (MeasureSpec.getMode(parentMaxHeightSpec) != MeasureSpec.UNSPECIFIED) {
            int maxHeight = Math.max(0, MeasureSpec.getSize(parentMaxHeightSpec) - marginHeight);
            parentMaxHeightSpec = MeasureSpec.makeMeasureSpec(
                    maxHeight, MeasureSpec.getMode(parentMaxHeightSpec));
        }
        if (viewLayoutParams.height > 0) {
            if (MeasureSpec.getMode(parentMaxHeightSpec) == MeasureSpec.UNSPECIFIED
                    || MeasureSpec.getSize(parentMaxHeightSpec)
                            > (viewLayoutParams.height + marginHeight)) {
                return MeasureSpec.makeMeasureSpec(viewLayoutParams.height, MeasureSpec.EXACTLY);
            }
            return parentMaxHeightSpec;
        } else {
            return parentMaxHeightSpec;
        }
    }

    // (Copied from LinearLayout.java API 27, removed references to private members, and simplified
    // slightly (ex. remove divider width))
    // THIS METHOD IS UNTESTED: PROCEED WITH CAUTION!
    @Override
    @SuppressLint("RtlHardcoded")
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final boolean isLayoutRtl = mIsRtlSupplier.get();
        final int paddingTop = getPaddingTop();

        int childTop;
        int childLeft;

        // Where bottom of child should go
        final int height = bottom - top;
        int childBottom = height - getPaddingBottom();

        // Space available for child
        int childSpace = height - paddingTop - getPaddingBottom();

        final int count = getChildCount();

        final int majorGravity = getGravity() & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK;
        final int minorGravity = getGravity() & Gravity.VERTICAL_GRAVITY_MASK;

        final int layoutDirection = getLayoutDirection();
        switch (Gravity.getAbsoluteGravity(majorGravity, layoutDirection)) {
            case Gravity.RIGHT:
                // totalContentWidth contains the padding already
                childLeft = getPaddingLeft() + right - left - mTotalContentWidth;
                break;

            case Gravity.CENTER_HORIZONTAL:
                // totalContentWidth contains the padding already
                childLeft = getPaddingLeft() + (right - left - mTotalContentWidth) / 2;
                break;

            case Gravity.LEFT:
            default:
                childLeft = getPaddingLeft();
                break;
        }

        int start = 0;
        int dir = 1;

        // In case of RTL, start drawing from the last child.
        if (isLayoutRtl) {
            start = count - 1;
            dir = -1;
        }

        for (int i = 0; i < count; i++) {
            final int childIndex = start + dir * i;
            final View child = getChildAt(childIndex);
            if (child.getVisibility() != GONE) {
                final int childWidth = child.getMeasuredWidth();
                final int childHeight = child.getMeasuredHeight();

                final LinearLayout.LayoutParams lp =
                        (LinearLayout.LayoutParams) child.getLayoutParams();

                int gravity = lp.gravity;
                if (gravity < 0) {
                    gravity = minorGravity;
                }

                switch (gravity & Gravity.VERTICAL_GRAVITY_MASK) {
                    case Gravity.TOP:
                        childTop = paddingTop + lp.topMargin;
                        break;

                    case Gravity.CENTER_VERTICAL:
                        childTop = paddingTop + ((childSpace - childHeight) / 2) + lp.topMargin
                                - lp.bottomMargin;
                        break;

                    case Gravity.BOTTOM:
                        childTop = childBottom - childHeight - lp.bottomMargin;
                        break;
                    default:
                        childTop = paddingTop;
                        break;
                }

                childLeft += lp.leftMargin;
                setChildFrame(child, childLeft, childTop, childWidth, childHeight);
                childLeft += childWidth + lp.rightMargin;
            }
        }
    }

    // (Copied from LinearLayout.java API 27)
    private void setChildFrame(View child, int left, int top, int width, int height) {
        child.layout(left, top, left + width, top + height);
    }

    /**
     * (Copied from LinearLayout.java API 27) Describes how the child views are positioned. Defaults
     * to GRAVITY_TOP. If this layout has a HORIZONTAL orientation, this controls the alignment of
     * the children.
     *
     * @param gravity See {@link android.view.Gravity}
     * @attr ref android.R.styleable#LinearLayout_gravity
     */
    @Override
    public void setGravity(int gravity) {
        if (mViewGravity != gravity) {
            if ((gravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) == 0) {
                gravity |= Gravity.START;
            }

            if ((gravity & Gravity.VERTICAL_GRAVITY_MASK) == 0) {
                gravity |= Gravity.TOP;
            }

            mViewGravity = gravity;
            requestLayout();
        }
    }

    /**
     * (Copied from LinearLayout.java API 27) Returns the current gravity. See {@link
     * android.view.Gravity}
     *
     * @return the current gravity.
     * @see #setGravity
     */
    @Override
    public int getGravity() {
        return mViewGravity;
    }

    // (Copied from LinearLayout.java API 27)
    @Override
    public void setHorizontalGravity(int horizontalGravity) {
        final int gravity = horizontalGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK;
        if ((mViewGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) != gravity) {
            mViewGravity = (mViewGravity & ~Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) | gravity;
            requestLayout();
        }
    }

    // (Copied from LinearLayout.java API 27)
    @Override
    public void setVerticalGravity(int verticalGravity) {
        final int gravity = verticalGravity & Gravity.VERTICAL_GRAVITY_MASK;
        if ((mViewGravity & Gravity.VERTICAL_GRAVITY_MASK) != gravity) {
            mViewGravity = (mViewGravity & ~Gravity.VERTICAL_GRAVITY_MASK) | gravity;
            requestLayout();
        }
    }

    @VisibleForTesting
    public boolean hasCollapsibleCells() {
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            if (((LayoutParams) view.getLayoutParams()).mIsCollapsible) {
                return true;
            }
        }
        return false;
    }

    /**
     * LayoutParams for GridRowViews. Essentially just LinearLayout.LayoutParams with a boolean to
     * keep track of whether this cell isCollapsible. Collapsible cells behave as WRAP_CONTENT when
     * they have sufficient room, but shrink when there is not enough room for them. Collapsible
     * cells take priority over weight cells; a weight cell will shrink to zero before the
     * collapsible cell starts to shrink.
     */
    public static class LayoutParams extends LinearLayout.LayoutParams {
        private final boolean mIsCollapsible;

        public LayoutParams(LayoutParams source) {
            super(source);
            mIsCollapsible = source.getIsCollapsible();
        }

        public LayoutParams(int width, int height, boolean isCollapsible) {
            this(width, height, 0.0f, isCollapsible);
        }

        /**
         * If a cell is collapsible, it doesn't make sense for it to also have weight (since all
         * weight cells are by definition collapsible, and have no intrinsic width).
         */
        public LayoutParams(int width, int height, float weight, boolean isCollapsible) {
            super(width, height, weight);
            if (isCollapsible && weight > 0) {
                isCollapsible = false;
            }
            if (width == MATCH_PARENT) {
                Logger.wtf(TAG, "GridRowView cells cannot have width MATCH_PARENT.");
            }
            this.mIsCollapsible = isCollapsible;
        }

        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            mIsCollapsible = false;
        }

        public LayoutParams(int width, int height) {
            super(width, height);
            mIsCollapsible = false;
        }

        public LayoutParams(int width, int height, float weight) {
            super(width, height, weight);
            mIsCollapsible = false;
        }

        public LayoutParams(ViewGroup.LayoutParams p) {
            super(p);
            mIsCollapsible = false;
        }

        public LayoutParams(MarginLayoutParams source) {
            super(source);
            mIsCollapsible = false;
        }

        public LayoutParams(LinearLayout.LayoutParams source) {
            super((MarginLayoutParams) source);
            weight = source.weight;
            gravity = source.gravity;
            mIsCollapsible = false;
        }

        public boolean getIsCollapsible() {
            return mIsCollapsible;
        }
    }
}
