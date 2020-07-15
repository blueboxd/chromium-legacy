// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;

import org.chromium.components.paintpreview.player.PlayerUserActionRecorder;

import javax.annotation.Nullable;

/**
 * Handles scaling of the top level frame for the paint preview player.
 */
public class PlayerFrameScaleController {
    private static final float MAX_SCALE_FACTOR = 5f;

    private float mInitialScaleFactor = 0f;
    private float mUncommittedScaleFactor = 0f;

    /** References to shared state. */
    private final PlayerFrameViewport mViewport;
    private final Size mContentSize;
    private final Matrix mBitmapScaleMatrix;
    /** Interface for calling shared methods on the mediator. */
    private final PlayerFrameMediatorDelegate mMediatorDelegate;
    private final Runnable mUserInteractionCallback;

    PlayerFrameScaleController(PlayerFrameViewport viewport, Size contentSize,
            Matrix bitmapScaleMatrix, PlayerFrameMediatorDelegate mediatorDelegate,
            @Nullable Runnable userInteractionCallback) {
        mViewport = viewport;
        mContentSize = contentSize;
        mBitmapScaleMatrix = bitmapScaleMatrix;
        mMediatorDelegate = mediatorDelegate;
        mUserInteractionCallback = userInteractionCallback;
    }

    /**
     * Calculates the initial scale factor for a given viewport width.
     * @param width The viewport width.
     */
    void calculateInitialScaleFactor(float width) {
        mInitialScaleFactor = width / ((float) mContentSize.getWidth());
    }

    /**
     * Gets the initial scale factor at the last computed viewport width.
     */
    float getInitialScaleFactor() {
        return mInitialScaleFactor;
    }

    /**
     * How scale for the paint preview player works.
     *
     * There are two reference frames:
     * - The currently loaded bitmaps, which changes as scaling happens.
     * - The viewport, which is static until scaling is finished.
     *
     * During {@link #scaleBy()} the gesture is still ongoing.
     *
     * On each scale gesture the |scaleFactor| is applied to |mUncommittedScaleFactor| which
     * accumulates the scale starting from the currently committed scale factor. This is
     * committed when {@link #scaleFinished()} event occurs. This is for the viewport reference
     * frame. |mViewport| also accumulates the transforms to track the translation behavior.
     *
     * |mBitmapScaleMatrix| tracks scaling from the perspective of the bitmaps. This is used to
     * transform the canvas the bitmaps are painted on such that scaled images can be shown
     * mid-gesture.
     *
     * Each subframe is updated with a new rect based on the interim scale factor and when the
     * matrix is set in {@link #setBitmapScaleMatrix()} the subframe matricies are recursively
     * updated.
     *
     * On {@link #scaleFinished()} the gesture is now considered finished.
     *
     * The final translation is applied to the viewport. The transform for the bitmaps (that is
     * |mBitmapScaleMatrix|) is cancelled.
     *
     * During {@link #updateVisuals()} new bitmaps are requested for the main frame and subframes
     * to improve quality.
     */
    boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        // This is filtered to only apply to the top level view upstream.
        if (mUncommittedScaleFactor == 0f) {
            mUncommittedScaleFactor = mViewport.getScale();
            mMediatorDelegate.onStartScaling();
        }
        // Don't scale outside of the acceptable range. The value is still accumulated such that the
        // continuous gesture feels smooth.
        final float lastUncommittedScaleFactor = mUncommittedScaleFactor;
        mUncommittedScaleFactor *= scaleFactor;
        // Compute a corrected and bounded scale factor when close to the max/min scale.
        if (mUncommittedScaleFactor < mInitialScaleFactor
                && lastUncommittedScaleFactor > mInitialScaleFactor) {
            scaleFactor = mInitialScaleFactor / lastUncommittedScaleFactor;
        } else if (mUncommittedScaleFactor > MAX_SCALE_FACTOR
                && lastUncommittedScaleFactor < MAX_SCALE_FACTOR) {
            scaleFactor = MAX_SCALE_FACTOR / lastUncommittedScaleFactor;
        } else if (mUncommittedScaleFactor > mInitialScaleFactor
                && lastUncommittedScaleFactor < mInitialScaleFactor) {
            scaleFactor = mUncommittedScaleFactor / mInitialScaleFactor;
        } else if (mUncommittedScaleFactor < MAX_SCALE_FACTOR
                && lastUncommittedScaleFactor > MAX_SCALE_FACTOR) {
            scaleFactor = mUncommittedScaleFactor / MAX_SCALE_FACTOR;
        } else if (mUncommittedScaleFactor < mInitialScaleFactor
                || lastUncommittedScaleFactor > MAX_SCALE_FACTOR) {
            return true;
        }
        final float correctedAggregateScaleFactor = lastUncommittedScaleFactor * scaleFactor;

        // TODO(crbug/1090804): trigger a fetch of new bitmaps periodically when zooming out.

        mViewport.scale(scaleFactor, focalPointX, focalPointY);
        mBitmapScaleMatrix.postScale(scaleFactor, scaleFactor, focalPointX, focalPointY);

        float[] bitmapScaleMatrixValues = new float[9];
        mBitmapScaleMatrix.getValues(bitmapScaleMatrixValues);

        // It is possible the scale pushed the viewport outside the content bounds. These new values
        // are forced to be within bounds.
        final float uncorrectedX = mViewport.getTransX();
        final float uncorrectedY = mViewport.getTransY();
        final float correctedX = Math.max(0f,
                Math.min(uncorrectedX,
                        mContentSize.getWidth() * correctedAggregateScaleFactor
                                - mViewport.getWidth()));
        final float correctedY = Math.max(0f,
                Math.min(uncorrectedY,
                        mContentSize.getHeight() * correctedAggregateScaleFactor
                                - mViewport.getHeight()));
        final int correctedXRounded = Math.abs(Math.round(correctedX));
        final int correctedYRounded = Math.abs(Math.round(correctedY));
        mMediatorDelegate.updateSubframes(new Rect(correctedXRounded, correctedYRounded,
                                                  correctedXRounded + mViewport.getWidth(),
                                                  correctedYRounded + mViewport.getHeight()),
                mUncommittedScaleFactor);

        if (uncorrectedX != correctedX || uncorrectedY != correctedY) {
            // This is the delta required to force the viewport to be inside the bounds of the
            // content.
            final float deltaX = uncorrectedX - correctedX;
            final float deltaY = uncorrectedY - correctedY;

            // Directly used the forced bounds of the viewport reference frame for the viewport
            // scale matrix.
            mViewport.setTrans(correctedX, correctedY);

            // For the bitmap matrix we only want the delta as its position will be different as the
            // coordinates are bitmap relative.
            bitmapScaleMatrixValues[Matrix.MTRANS_X] += deltaX;
            bitmapScaleMatrixValues[Matrix.MTRANS_Y] += deltaY;
            mBitmapScaleMatrix.setValues(bitmapScaleMatrixValues);
        }

        mMediatorDelegate.setBitmapScaleMatrix(mBitmapScaleMatrix, correctedAggregateScaleFactor);
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        return true;
    }

    /**
     * Called when scaling is finished to finalize the scaling.
     * @param scaleFactor The final scale event's scale factor.
     * @param focalPointX The final scale event's focal point in the x-axis.
     * @param focalPointY The final scale event's focal point in the y-axis.
     * @return Whether the scale event was consumed.
     */
    boolean scaleFinished(float scaleFactor, float focalPointX, float focalPointY) {
        // All correction/scaling happens in scaleBy() here we just update the mediator.
        mMediatorDelegate.resetScaleFactorOfAllSubframes();
        mMediatorDelegate.updateVisuals(true);
        mMediatorDelegate.forceRedrawVisibleSubframes();
        PlayerUserActionRecorder.recordZoom();
        mUncommittedScaleFactor = 0f;
        return true;
    }
}
