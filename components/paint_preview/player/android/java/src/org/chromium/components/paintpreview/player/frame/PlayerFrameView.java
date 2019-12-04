// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

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
    private List<Pair<View, Rect>> mSubFrames;

    /**
     * @param context Used for initialization.
     * @param canDetectZoom Whether this {@link View} should detect zoom (scale) gestures.
     * @param playerFrameViewDelegate The interface used for forwarding events.
     */
    PlayerFrameView(@NonNull Context context, boolean canDetectZoom,
            PlayerFrameViewDelegate playerFrameViewDelegate) {
        super(context);
        mDelegate = playerFrameViewDelegate;
        mBitmapPainter = new PlayerFrameBitmapPainter(this::invalidate);
        mGestureDetector =
                new PlayerFrameGestureDetector(context, canDetectZoom, playerFrameViewDelegate);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        mDelegate.setLayoutDimensions(getWidth(), getHeight());

        for (int i = 0; i < mSubFrames.size(); i++) {
            View childView = getChildAt(i);
            if (childView == null) {
                continue;
            }

            Rect childRect = mSubFrames.get(i).second;
            childView.layout(childRect.left, childRect.top, childRect.right, childRect.bottom);
        }
    }

    /**
     * Updates the sub-frames that this {@link PlayerFrameView} should display, along with their
     * coordinates.
     * @param subFrames List of all sub-frames, along with their coordinates.
     */
    void updateSubFrames(List<Pair<View, Rect>> subFrames) {
        // TODO(mahmoudi): Removing all views every time is not smart. Only remove the views that
        // are not in subFrames.first.
        mSubFrames = subFrames;
        removeAllViews();
        for (int i = 0; i < subFrames.size(); i++) {
            addView(subFrames.get(i).first, i);
        }
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        mBitmapPainter.updateViewPort(left, top, right, bottom);
    }

    void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        mBitmapPainter.updateBitmapMatrix(bitmapMatrix);
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
