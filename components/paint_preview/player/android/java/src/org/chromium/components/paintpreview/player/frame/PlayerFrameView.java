// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import java.util.List;

/**
 * Responsible for detecting touch gestures, displaying the content of a frame and its sub-frames.
 * {@link PlayerFrameBitmapPainter} is used for drawing the contents.
 * Sub-frames are represented with individual {@link View}s. {@link #mSubFrames} contains the list
 * of all sub-frames and their relative positions.
 */
class PlayerFrameView extends FrameLayout {
    private PlayerFrameBitmapPainter mBitmapPainter;
    private PlayerFrameGestureDetector mGestureDetector;
    private PlayerFrameViewDelegate mDelegate;
    private List<View> mSubFrameViews;
    private List<Rect> mSubFrameRects;

    /**
     * @param context Used for initialization.
     * @param canDetectZoom Whether this {@link View} should detect zoom (scale) gestures.
     * @param playerFrameViewDelegate The interface used for forwarding events.
     */
    PlayerFrameView(@NonNull Context context, boolean canDetectZoom,
            PlayerFrameViewDelegate playerFrameViewDelegate) {
        super(context);
        setWillNotDraw(false);
        mDelegate = playerFrameViewDelegate;
        mBitmapPainter = new PlayerFrameBitmapPainter(this::invalidate);
        mGestureDetector =
                new PlayerFrameGestureDetector(context, canDetectZoom, playerFrameViewDelegate);
    }

    PlayerFrameGestureDetector getGestureDetector() {
        return mGestureDetector;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        mDelegate.setLayoutDimensions(getWidth(), getHeight());
    }

    /**
     * Updates the sub-frame views that this {@link PlayerFrameView} should display.
     * @param subFrameViews List of all sub-frame views.
     */
    void updateSubFrameViews(List<View> subFrameViews) {
        mSubFrameViews = subFrameViews;
    }

    /**
     * Updates clip rects for sub-frames that this {@link PlayerFrameView} should display.
     * @param subFrameRects List of all sub-frames clip rects.
     */
    void updateSubFrameRects(List<Rect> subFrameRects) {
        mSubFrameRects = subFrameRects;
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        mBitmapPainter.updateViewPort(left, top, right, bottom);

        // Remove all views if there are no sub-frames.
        if (mSubFrameViews == null || mSubFrameRects == null) {
            removeAllViews();
            return;
        }

        // Layout the sub-frames.
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getParent() == null) {
                addView(mSubFrameViews.get(i));
            } else if (mSubFrameViews.get(i).getParent() != this) {
                throw new IllegalStateException("Sub-frame view already has a parent.");
            }
            Rect layoutRect = mSubFrameRects.get(i);
            mSubFrameViews.get(i).layout(
                    layoutRect.left, layoutRect.top, layoutRect.right, layoutRect.bottom);
        }

        for (int i = 0; i < getChildCount(); i++) {
            if (!mSubFrameViews.contains(getChildAt(i))) {
                removeViewAt(i);
                --i;
            }
        }
    }

    void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        mBitmapPainter.updateBitmapMatrix(bitmapMatrix);
    }

    void updateTileDimensions(int[] tileDimensions) {
        mBitmapPainter.updateTileDimensions(tileDimensions);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        mBitmapPainter.onDraw(canvas);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mGestureDetector.onTouchEvent(event) || super.onTouchEvent(event);
    }
}
