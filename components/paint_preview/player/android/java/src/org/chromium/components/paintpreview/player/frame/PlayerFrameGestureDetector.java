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
    private PlayerFrameGestureDetector mParentGestureDetector;
    /**
     * Last horizontal scroll distance that was detected by this {@link PlayerFrameGestureDetector}
     * and consumed by {@link #mParentGestureDetector}.
     */
    private float mLastParentScrollX;
    /**
     * Last vertical scroll distance that was detected by this {@link PlayerFrameGestureDetector}
     * and consumed by {@link #mParentGestureDetector}.
     */
    private float mLastParentScrollY;

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
     * Sets the {@link PlayerFrameGestureDetector} that corresponds to the parent view of this
     * {@link PlayerFrameGestureDetector}'s view. This is used for forwarding unconsumed touch
     * events.
     */
    void setParentGestureDetector(PlayerFrameGestureDetector parentGestureDetector) {
        mParentGestureDetector = parentGestureDetector;
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
        return true;
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
        if (mPlayerFrameViewDelegate.scrollBy(distanceX, distanceY)) {
            mLastParentScrollX = 0f;
            mLastParentScrollY = 0f;
            return true;
        }

        // We need to keep track of the distance passed to the parent
        // {@link PlayerFrameGestureDetector} and accumulate them for the following events. This is
        // because if the parent view scrolls, the coordinates of the future touch events that this
        // view received will be transformed since the View associated with this
        // {@link PlayerFrameGestureDetector} moves along with the parent.
        mLastParentScrollX += distanceX;
        mLastParentScrollY += distanceY;
        if (mParentGestureDetector != null
                && mParentGestureDetector.onScroll(
                        e1, e2, mLastParentScrollX, mLastParentScrollY)) {
            return true;
        }
        mLastParentScrollX = 0f;
        mLastParentScrollY = 0f;
        return false;
    }

    @Override
    public void onLongPress(MotionEvent e) {}

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (mPlayerFrameViewDelegate.onFling(velocityX, velocityY)) return true;

        if (mParentGestureDetector != null) {
            return mParentGestureDetector.onFling(e1, e2, velocityX, velocityY);
        }
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
