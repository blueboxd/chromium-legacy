// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.content.Context;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;

import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders.Edges;

/**
 * Shape used to draw borders. Uses offsets to push the border out of the drawing bounds if it is
 * not specified.
 */
public class BorderDrawable extends ShapeDrawable {
    private final boolean mHasLeftBorder;
    private final boolean mHasRightBorder;
    private final boolean mHasTopBorder;
    private final boolean mHasBottomBorder;
    private final int mOffsetToHideLeft;
    private final int mOffsetToHideRight;
    private final int mOffsetToHideTop;
    private final int mOffsetToHideBottom;
    private final int mBorderWidth;

    public BorderDrawable(Context context, Borders borders, float[] cornerRadii, boolean isRtL) {
        this(context, borders, cornerRadii, isRtL, /* width= */ 0, /* height= */ 0);
    }

    // Doesn't like calls to getPaint()
    @SuppressWarnings("nullness")
    public BorderDrawable(Context context, Borders borders, float[] cornerRadii, boolean isRtL,
            int width, int height) {
        super(new RoundRectShape(cornerRadii, null, null));

        mBorderWidth = (int) LayoutUtils.dpToPx(borders.getWidth(), context);

        // Calculate the offsets which push the border outside the view, making it invisible
        int bitmask = borders.getBitmask();
        if (bitmask == 0 || bitmask == 15) {
            // All borders are visible
            mHasLeftBorder = true;
            mHasRightBorder = true;
            mHasTopBorder = true;
            mHasBottomBorder = true;
            mOffsetToHideLeft = 0;
            mOffsetToHideRight = 0;
            mOffsetToHideTop = 0;
            mOffsetToHideBottom = 0;
        } else {
            int leftEdge = isRtL ? Edges.END.getNumber() : Edges.START.getNumber();
            int rightEdge = isRtL ? Edges.START.getNumber() : Edges.END.getNumber();
            mHasLeftBorder = (bitmask & leftEdge) != 0;
            mHasRightBorder = (bitmask & rightEdge) != 0;
            mHasTopBorder = (bitmask & Edges.TOP.getNumber()) != 0;
            mHasBottomBorder = (bitmask & Edges.BOTTOM.getNumber()) != 0;
            mOffsetToHideLeft = mHasLeftBorder ? 0 : -mBorderWidth;
            mOffsetToHideRight = mHasRightBorder ? 0 : mBorderWidth;
            mOffsetToHideTop = mHasTopBorder ? 0 : -mBorderWidth;
            mOffsetToHideBottom = mHasBottomBorder ? 0 : mBorderWidth;
        }
        getPaint().setStyle(Paint.Style.STROKE);
        // Multiply the width by two - the centerline of the stroke will be the edge of the view, so
        // half of the stroke will be outside the view. In order for the visible portion to have the
        // correct width, the full stroke needs to be twice as wide.
        // For rounded corners, this relies on the containing FrameLayout to crop the outside half
        // of the rounded border; otherwise, the border would get thicker on the corners.
        getPaint().setStrokeWidth(mBorderWidth * 2);
        getPaint().setColor(borders.getColor());
    }

    @Override
    public void setBounds(int left, int top, int right, int bottom) {
        super.setBounds(left + mOffsetToHideLeft, top + mOffsetToHideTop,
                right + mOffsetToHideRight, bottom + mOffsetToHideBottom);
    }

    @Override
    public void setBounds(Rect bounds) {
        setBounds(bounds.left, bounds.top, bounds.right, bounds.bottom);
    }
}
