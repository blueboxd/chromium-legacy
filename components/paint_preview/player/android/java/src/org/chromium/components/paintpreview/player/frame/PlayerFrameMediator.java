// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.widget.OverScroller;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.OverscrollHandler;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles the business logic for the player frame component. Concretely, this class is responsible
 * for:
 * <ul>
 * <li>Maintaining a viewport {@link Rect} that represents the current user-visible section of this
 * frame. The dimension of the viewport is constant and is equal to the initial values received on
 * {@link #setLayoutDimensions}.</li>
 * <li>Constructing a matrix of {@link Bitmap} tiles that represents the content of this frame for a
 * given scale factor. Each tile is as big as the view port.</li>
 * <li>Requesting bitmaps from Paint Preview compositor.</li>
 * <li>Updating the viewport on touch gesture notifications (scrolling and scaling).<li/>
 * <li>Determining which sub-frames are visible given the current viewport and showing them.<li/>
 * </ul>
 */
class PlayerFrameMediator implements PlayerFrameViewDelegate, PlayerFrameMediatorDelegate {
    /** The GUID associated with the frame that this class is representing. */
    private final UnguessableToken mGuid;
    /** The size of the content inside this frame, at a scale factor of 1. */
    private final Size mContentSize;
    /**
     * Contains all {@link View}s corresponding to this frame's sub-frames.
     */
    private final List<View> mSubFrameViews = new ArrayList<>();
    /**
     * Contains all clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameRects = new ArrayList<>();
    /**
     * Contains all mediators corresponding to this frame's sub-frames.
     */
    private final List<PlayerFrameMediator> mSubFrameMediators = new ArrayList<>();
    /**
     * Contains scaled clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameScaledRects = new ArrayList<>();

    private final PropertyModel mModel;
    private final PlayerCompositorDelegate mCompositorDelegate;
    /** The viewport of this frame. */
    private final PlayerFrameViewport mViewport;

    /** Handles scaling of bitmaps. */
    private final Matrix mBitmapScaleMatrix = new Matrix();

    /** Handles scrolling. */
    private final PlayerFrameScrollController mScrollController;
    /** Handles scaling. */
    private final PlayerFrameScaleController mScaleController;

    private final PlayerFrameBitmapStateController mBitmapStateController;

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            PlayerFrameViewport viewport, OverScroller scroller,
            @Nullable Runnable userInteractionCallback, UnguessableToken frameGuid,
            int contentWidth, int contentHeight, int initialScrollX, int initialScrollY) {
        mModel = model;
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);

        mCompositorDelegate = compositorDelegate;
        mViewport = viewport;
        mGuid = frameGuid;
        mContentSize = new Size(contentWidth, contentHeight);
        mBitmapStateController = new PlayerFrameBitmapStateController(
                mGuid, mViewport, mContentSize, mCompositorDelegate, this);
        mScrollController = new PlayerFrameScrollController(
                scroller, mViewport, mContentSize, this, userInteractionCallback);
        mScaleController = new PlayerFrameScaleController(
                mViewport, mContentSize, mBitmapScaleMatrix, this, userInteractionCallback);
        mViewport.offset(initialScrollX, initialScrollY);
        mViewport.setScale(0f);
    }

    @VisibleForTesting
    PlayerFrameBitmapStateController getBitmapStateControllerForTest() {
        return mBitmapStateController;
    }

    void updateViewportSize(int width, int height, float scaleFactor) {
        if (width <= 0 || height <= 0) return;

        // Ensure the viewport is within the bounds of the content.
        final int left = Math.max(0,
                Math.min(Math.round(mViewport.getTransX()),
                        Math.round(mContentSize.getWidth() * scaleFactor) - width));
        final int top = Math.max(0,
                Math.min(Math.round(mViewport.getTransY()),
                        Math.round(mContentSize.getHeight() * scaleFactor) - height));
        mViewport.setTrans(left, top);
        mViewport.setSize(width, height);
        final float oldScaleFactor = mViewport.getScale();
        mViewport.setScale(scaleFactor);
        updateVisuals(oldScaleFactor != scaleFactor);
    }

    /**
     * Adds a new sub-frame to this frame.
     * @param subFrameView The {@link View} associated with the sub-frame.
     * @param clipRect     The bounds of the sub-frame, relative to this frame.
     * @param mediator     The mediator of the sub-frame.
     */
    void addSubFrame(View subFrameView, Rect clipRect, PlayerFrameMediator mediator) {
        mSubFrameViews.add(subFrameView);
        mSubFrameRects.add(clipRect);
        mSubFrameMediators.add(mediator);
        mSubFrameScaledRects.add(new Rect());
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, mSubFrameViews);
        mModel.set(PlayerFrameProperties.SUBFRAME_RECTS, mSubFrameScaledRects);
    }

    void setBitmapScaleMatrixOfSubframe(Matrix matrix, float scaleFactor) {
        // Don't update the subframes if the matrix is identity as it will be forcibly recalculated.
        if (!matrix.isIdentity()) {
            updateSubframes(mViewport.asRect(), scaleFactor);
        }
        setBitmapScaleMatrix(matrix, scaleFactor);
    }

    /**
     * Sets the overscroll-to-refresh handler on the {@link mScrollController}. This cannot be
     * created at construction of this object as it needs to be created on top of the view
     * hierarchy to show the animation.
     */
    void setOverscrollHandler(OverscrollHandler overscrollHandler) {
        mScrollController.setOverscrollHandler(overscrollHandler);
    }

    // PlayerFrameViewDelegate

    @Override
    public void setLayoutDimensions(int width, int height) {
        // Don't trigger a re-draw if we are actively scaling.
        if (!mBitmapScaleMatrix.isIdentity()) {
            mViewport.setSize(width, height);
            return;
        }

        // Set initial scale so that content width fits within the layout dimensions.
        mScaleController.calculateInitialScaleFactor(width);
        final float scaleFactor = mViewport.getScale();
        updateViewportSize(width, height,
                (scaleFactor == 0f) ? mScaleController.getInitialScaleFactor() : scaleFactor);
    }

    @Override
    public boolean scrollBy(float distanceX, float distanceY) {
        return mScrollController.scrollBy(distanceX, distanceY);
    }

    @Override
    public boolean onFling(float velocityX, float velocityY) {
        return mScrollController.onFling(velocityX, velocityY);
    }

    @Override
    public void onRelease() {
        mScrollController.onRelease();
    }

    @Override
    public boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        return mScaleController.scaleBy(scaleFactor, focalPointX, focalPointY);
    }

    @Override
    public boolean scaleFinished(float scaleFactor, float focalPointX, float focalPointY) {
        return mScaleController.scaleFinished(scaleFactor, focalPointX, focalPointY);
    }

    @Override
    public void onClick(int x, int y) {
        // x and y are in the View's coordinate system (scaled). This needs to be adjusted to the
        // absolute coordinate system for hit testing.
        final float scaleFactor = mViewport.getScale();
        mCompositorDelegate.onClick(mGuid,
                Math.round((float) (mViewport.getTransX() + x) / scaleFactor),
                Math.round((float) (mViewport.getTransY() + y) / scaleFactor));
    }

    // PlayerFrameMediatorDelegate

    @Override
    public void onStartScaling() {
        mBitmapStateController.invalidateLoadingBitmaps();
    }

    @Override
    public void onSwapState() {
        PlayerFrameBitmapState bitmapState = mBitmapStateController.getBitmapState(false);
        mBitmapScaleMatrix.reset();
        setBitmapScaleMatrix(mBitmapScaleMatrix, 1f);
        mModel.set(PlayerFrameProperties.TILE_DIMENSIONS, bitmapState.getTileDimensions());
        mModel.set(PlayerFrameProperties.VIEWPORT, mViewport.asRect());
        mModel.set(PlayerFrameProperties.BITMAP_MATRIX, bitmapState.getMatrix());
    }

    @Override
    public void offsetBitmapScaleMatrix(float dx, float dy) {
        // If we are still waiting on new bitmaps after a scale operation, the scroll should scroll
        // the bitmaps we currently have. In order to do so we apply an opposite transform to the
        // bitmaps that are shown on the screen.
        if (!mBitmapScaleMatrix.isIdentity()) {
            float[] bitmapScaleMatrixValues = new float[9];
            mBitmapScaleMatrix.getValues(bitmapScaleMatrixValues);
            bitmapScaleMatrixValues[Matrix.MTRANS_X] -= dx;
            bitmapScaleMatrixValues[Matrix.MTRANS_Y] -= dy;
            mBitmapScaleMatrix.setValues(bitmapScaleMatrixValues);
            setBitmapScaleMatrix(mBitmapScaleMatrix, mViewport.getScale());
        }
    }

    /**
     * Called when the viewport is moved or the scale factor is changed. Updates the viewport
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param scaleUpdated Whether the scale was updated.
     */
    @Override
    public void updateVisuals(boolean scaleUpdated) {
        final float scaleFactor = mViewport.getScale();
        PlayerFrameBitmapState activeLoadingState =
                mBitmapStateController.getBitmapState(scaleUpdated);
        Rect viewportRect = mViewport.asRect();
        updateSubframes(viewportRect, scaleFactor);
        // Let the view know |mViewport| changed. PropertyModelChangeProcessor is smart about
        // this and will only update the view if |mViewport|'s rect is actually changed.
        if (mBitmapStateController.isVisible(activeLoadingState)) {
            mModel.set(
                    PlayerFrameProperties.TILE_DIMENSIONS, activeLoadingState.getTileDimensions());
            mModel.set(PlayerFrameProperties.VIEWPORT, viewportRect);
        }

        // Clear the required bitmaps matrix. It will be updated in #requestBitmapForTile.
        activeLoadingState.clearRequiredBitmaps();

        // Request bitmaps for tiles inside the view port that don't already have a bitmap.
        activeLoadingState.requestBitmapForRect(viewportRect);
    }

    @Override
    public void resetScaleFactorOfAllSubframes() {
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).resetScaleFactor();
        }
    }

    @Override
    public void forceRedrawVisibleSubframes() {
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }
    }

    @Override
    public void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        mModel.set(PlayerFrameProperties.BITMAP_MATRIX, bitmapMatrix);
    }

    @Override
    public void updateSubframes(Rect viewport, float scaleFactor) {
        for (int i = 0; i < mSubFrameRects.size(); i++) {
            Rect subFrameScaledRect = mSubFrameScaledRects.get(i);
            scaleRect(mSubFrameRects.get(i), subFrameScaledRect, scaleFactor);
            if (!Rect.intersects(subFrameScaledRect, viewport)) {
                mSubFrameViews.get(i).setVisibility(View.GONE);
                subFrameScaledRect.set(0, 0, 0, 0);
                continue;
            }

            int transformedLeft = subFrameScaledRect.left - viewport.left;
            int transformedTop = subFrameScaledRect.top - viewport.top;
            subFrameScaledRect.set(transformedLeft, transformedTop,
                    transformedLeft + subFrameScaledRect.width(),
                    transformedTop + subFrameScaledRect.height());
            mSubFrameViews.get(i).setVisibility(View.VISIBLE);
        }
        mModel.set(PlayerFrameProperties.SUBFRAME_RECTS, mSubFrameScaledRects);
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, mSubFrameViews);
    }

    @Override
    public void setBitmapScaleMatrix(Matrix matrix, float scaleFactor) {
        float[] matrixValues = new float[9];
        matrix.getValues(matrixValues);
        mBitmapScaleMatrix.setValues(matrixValues);
        Matrix childBitmapScaleMatrix = new Matrix();
        childBitmapScaleMatrix.setScale(
                matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).setBitmapScaleMatrixOfSubframe(
                    childBitmapScaleMatrix, scaleFactor);
        }
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);
    }

    // Internal methods

    @VisibleForTesting
    void resetScaleFactor() {
        // Set scale factor to 0 so subframes get the correct scale factor on scale completion.
        mViewport.setScale(0f);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).resetScaleFactor();
        }
    }

    @VisibleForTesting
    void forceRedraw() {
        mScaleController.calculateInitialScaleFactor(mViewport.getWidth());
        final float scaleFactor = mViewport.getScale();
        mViewport.setScale(
                (scaleFactor == 0f) ? mScaleController.getInitialScaleFactor() : scaleFactor);
        updateVisuals(true);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }
    }

    private void scaleRect(Rect inRect, Rect outRect, float scaleFactor) {
        outRect.set((int) (((float) inRect.left) * scaleFactor),
                (int) (((float) inRect.top) * scaleFactor),
                (int) (((float) inRect.right) * scaleFactor),
                (int) (((float) inRect.bottom) * scaleFactor));
    }
}
