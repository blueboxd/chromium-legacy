// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import android.content.Context;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;

import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.search.now.ui.piet.StylesProto.Borders;
import com.google.search.now.ui.piet.StylesProto.Borders.Edges;

/**
 * Shape used to draw borders. Uses offsets to push the border out of the drawing bounds if it is
 * not specified.
 */
public class BorderDrawable extends ShapeDrawable {
    private final boolean hasLeftBorder;
    private final boolean hasRightBorder;
    private final boolean hasTopBorder;
    private final boolean hasBottomBorder;
    private final int offsetToHideLeft;
    private final int offsetToHideRight;
    private final int offsetToHideTop;
    private final int offsetToHideBottom;
    private final int borderWidth;

    public BorderDrawable(Context context, Borders borders, float[] cornerRadii, boolean isRtL) {
        this(context, borders, cornerRadii, isRtL, /* width= */ 0, /* height= */ 0);
    }

    // Doesn't like calls to getPaint()
    @SuppressWarnings("initialization")
    public BorderDrawable(Context context, Borders borders, float[] cornerRadii, boolean isRtL,
            int width, int height) {
        super(new RoundRectShape(cornerRadii, null, null));

        borderWidth = (int) LayoutUtils.dpToPx(borders.getWidth(), context);

        // Calculate the offsets which push the border outside the view, making it invisible
        int bitmask = borders.getBitmask();
        if (bitmask == 0 || bitmask == 15) {
            // All borders are visible
            hasLeftBorder = true;
            hasRightBorder = true;
            hasTopBorder = true;
            hasBottomBorder = true;
            offsetToHideLeft = 0;
            offsetToHideRight = 0;
            offsetToHideTop = 0;
            offsetToHideBottom = 0;
        } else {
            int leftEdge = isRtL ? Edges.END.getNumber() : Edges.START.getNumber();
            int rightEdge = isRtL ? Edges.START.getNumber() : Edges.END.getNumber();
            hasLeftBorder = (bitmask & leftEdge) != 0;
            hasRightBorder = (bitmask & rightEdge) != 0;
            hasTopBorder = (bitmask & Edges.TOP.getNumber()) != 0;
            hasBottomBorder = (bitmask & Edges.BOTTOM.getNumber()) != 0;
            offsetToHideLeft = hasLeftBorder ? 0 : -borderWidth;
            offsetToHideRight = hasRightBorder ? 0 : borderWidth;
            offsetToHideTop = hasTopBorder ? 0 : -borderWidth;
            offsetToHideBottom = hasBottomBorder ? 0 : borderWidth;
        }
        getPaint().setStyle(Paint.Style.STROKE);
        // Multiply the width by two - the centerline of the stroke will be the edge of the view, so
        // half of the stroke will be outside the view. In order for the visible portion to have the
        // correct width, the full stroke needs to be twice as wide.
        // For rounded corners, this relies on the containing FrameLayout to crop the outside half
        // of the rounded border; otherwise, the border would get thicker on the corners.
        getPaint().setStrokeWidth(borderWidth * 2);
        getPaint().setColor(borders.getColor());
    }

    @Override
    public void setBounds(int left, int top, int right, int bottom) {
        super.setBounds(left + offsetToHideLeft, top + offsetToHideTop, right + offsetToHideRight,
                bottom + offsetToHideBottom);
    }

    @Override
    public void setBounds(Rect bounds) {
        setBounds(bounds.left, bounds.top, bounds.right, bounds.bottom);
    }
}
