// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

/**
 * Detects scroll, fling, and scale gestures on calls to {@link #onTouchEvent} and reports back to
 * the provided {@link PlayerFrameViewDelegate}.
 */
class PlayerFrameGestureDetector
        implements GestureDetector.OnGestureListener, ScaleGestureDetector.OnScaleGestureListener {
    private GestureDetector mGestureDetector;
    private ScaleGestureDetector mScaleGestureDetector;
    private boolean mCanDetectZoom;
    private PlayerFrameViewDelegate mPlayerFrameViewDelegate;

    /**
     * @param context Used for initializing {@link GestureDetector} and
     * {@link ScaleGestureDetector}.
     * @param canDetectZoom Whether this {@link PlayerFrameGestureDetector} should detect scale
     * gestures.
     * @param playerFrameViewDelegate The delegate used when desired gestured are detected.
     */
    PlayerFrameGestureDetector(Context context, boolean canDetectZoom,
            PlayerFrameViewDelegate playerFrameViewDelegate) {
        mGestureDetector = new GestureDetector(context, this);
        mScaleGestureDetector = new ScaleGestureDetector(context, this);
        mCanDetectZoom = canDetectZoom;
        mPlayerFrameViewDelegate = playerFrameViewDelegate;
    }

    /**
     * This should be called on every touch event.
     * @return Whether the event was consumed.
     */
    boolean onTouchEvent(MotionEvent event) {
        if (mCanDetectZoom) {
            mScaleGestureDetector.onTouchEvent(event);
        }

        return mGestureDetector.onTouchEvent(event);
    }

    @Override
    public boolean onDown(MotionEvent e) {
        return false;
    }

    @Override
    public void onShowPress(MotionEvent e) {}

    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        mPlayerFrameViewDelegate.onClick((int) e.getX(), (int) e.getY());
        return true;
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        return mPlayerFrameViewDelegate.scrollBy(distanceX, distanceY);
    }

    @Override
    public void onLongPress(MotionEvent e) {}

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        return false;
    }

    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        assert mCanDetectZoom;
        return mPlayerFrameViewDelegate.scaleBy(
                detector.getScaleFactor(), detector.getFocusX(), detector.getFocusY());
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        assert mCanDetectZoom;
        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {}
}
