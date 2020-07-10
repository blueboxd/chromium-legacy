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

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.OverscrollHandler;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

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
    /** Dimension of tiles. */
    private int[] mTileDimensions;
    /** Bitmaps that make up the content of this frame. */
    private Bitmap[][] mBitmapMatrix;
    /** Whether a request for a bitmap tile is pending. */
    private boolean[][] mPendingBitmapRequests;
    /**
     * Whether we currently need a bitmap tile. This is used for deleting bitmaps that we don't
     * need and freeing up memory.
     */
    @VisibleForTesting
    boolean[][] mRequiredBitmaps;

    /** Handles scaling of bitmaps. */
    private final Matrix mBitmapScaleMatrix = new Matrix();

    /** Handles scrolling. */
    private final PlayerFrameScrollController mScrollController;
    /** Handles scaling. */
    private final PlayerFrameScaleController mScaleController;

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
        mScrollController = new PlayerFrameScrollController(
                scroller, mViewport, mContentSize, this, userInteractionCallback);
        mScaleController = new PlayerFrameScaleController(
                mViewport, mContentSize, mBitmapScaleMatrix, this, userInteractionCallback);
        mViewport.offset(initialScrollX, initialScrollY);
        mViewport.setScale(0f);
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

    /**
     * Called when the viewport is moved or the scale factor is changed. Updates the viewport
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param scaleUpdated Whether the scale was updated.
     */
    @Override
    public void updateVisuals(boolean scaleUpdated) {
        final float scaleFactor = mViewport.getScale();
        if (scaleUpdated || mBitmapMatrix == null) {
            // Each tile is as big as the initial view port. Here we determine the number of
            // columns and rows for the current scale factor.
            int rows = (int) Math.ceil(
                    (mContentSize.getHeight() * scaleFactor) / mViewport.getHeight());
            int cols =
                    (int) Math.ceil((mContentSize.getWidth() * scaleFactor) / mViewport.getWidth());
            mTileDimensions = new int[] {mViewport.getWidth(), mViewport.getHeight()};
            mBitmapMatrix = new Bitmap[rows][cols];
            mPendingBitmapRequests = new boolean[rows][cols];
            mRequiredBitmaps = new boolean[rows][cols];
        }

        Rect viewportRect = mViewport.asRect();
        updateSubframes(viewportRect, mViewport.getScale());
        // Let the view know |mViewport| changed. PropertyModelChangeProcessor is smart about
        // this and will only update the view if |mViewport|'s rect is actually changed.
        mModel.set(PlayerFrameProperties.TILE_DIMENSIONS, mTileDimensions);
        mModel.set(PlayerFrameProperties.VIEWPORT, mViewport.asRect());

        // Clear the required bitmaps matrix. It will be updated in #requestBitmapForTile.
        for (int row = 0; row < mRequiredBitmaps.length; row++) {
            for (int col = 0; col < mRequiredBitmaps[row].length; col++) {
                mRequiredBitmaps[row][col] = false;
            }
        }

        // Request bitmaps for tiles inside the view port that don't already have a bitmap.
        final int tileWidth = mTileDimensions[0];
        final int tileHeight = mTileDimensions[1];
        final int colStart = Math.max(0, (int) Math.floor((double) viewportRect.left / tileWidth));
        final int colEnd = Math.min(mRequiredBitmaps[0].length,
                (int) Math.ceil((double) viewportRect.right / tileWidth));
        final int rowStart = Math.max(0, (int) Math.floor((double) viewportRect.top / tileHeight));
        final int rowEnd = Math.min(mRequiredBitmaps.length,
                (int) Math.ceil((double) viewportRect.bottom / tileHeight));
        for (int col = colStart; col < colEnd; col++) {
            for (int row = rowStart; row < rowEnd; row++) {
                int tileLeft = col * tileWidth;
                int tileTop = row * tileHeight;
                requestBitmapForTile(
                        tileLeft, tileTop, tileWidth, tileHeight, row, col, scaleFactor);
            }
        }

        // Request bitmaps for adjacent tiles that are not currently in the view port. The reason
        // that we do this in a separate loop is to make sure bitmaps for tiles inside the view port
        // are fetched first.
        for (int col = colStart; col < colEnd; col++) {
            for (int row = rowStart; row < rowEnd; row++) {
                requestBitmapForAdjacentTiles(tileWidth, tileHeight, row, col, scaleFactor);
            }
        }

        // If the scale factor is changed, the view should get the correct bitmap matrix.
        // TODO(crbug/1090804): "Double buffer" this such that there is no period where there is a
        // blank screen between scale finishing and new bitmaps being fetched.
        if (scaleUpdated) {
            mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix);
        }
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
    public void updateSubframes(Rect viewport, float scaleFactor) {
        for (int i = 0; i < mSubFrameRects.size(); i++) {
            Rect subFrameScaledRect = mSubFrameScaledRects.get(i);
            scaleRect(mSubFrameRects.get(i), subFrameScaledRect, scaleFactor);
            if (!Rect.intersects(subFrameScaledRect, viewport)) {
                mSubFrameViews.get(i).setVisibility(View.GONE);
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

    private void requestBitmapForAdjacentTiles(
            int tileWidth, int tileHeight, int row, int col, float scaleFactor) {
        if (mBitmapMatrix == null) return;

        if (row > 0) {
            requestBitmapForTile(col * tileWidth, (row - 1) * tileHeight, tileWidth, tileHeight,
                    row - 1, col, scaleFactor);
        }
        if (row < mBitmapMatrix.length - 1) {
            requestBitmapForTile(col * tileWidth, (row + 1) * tileHeight, tileWidth, tileHeight,
                    row + 1, col, scaleFactor);
        }
        if (col > 0) {
            requestBitmapForTile((col - 1) * tileWidth, row * tileHeight, tileWidth, tileHeight,
                    row, col - 1, scaleFactor);
        }
        if (col < mBitmapMatrix[row].length - 1) {
            requestBitmapForTile((col + 1) * tileWidth, row * tileHeight, tileWidth, tileHeight,
                    row, col + 1, scaleFactor);
        }
    }

    private void requestBitmapForTile(
            int x, int y, int width, int height, int row, int col, float scaleFactor) {
        if (mRequiredBitmaps == null) return;

        mRequiredBitmaps[row][col] = true;
        if (mBitmapMatrix == null || mPendingBitmapRequests == null
                || mBitmapMatrix[row][col] != null || mPendingBitmapRequests[row][col]) {
            return;
        }

        BitmapRequestHandler bitmapRequestHandler = new BitmapRequestHandler(row, col, scaleFactor);
        mPendingBitmapRequests[row][col] = true;
        mCompositorDelegate.requestBitmap(mGuid, new Rect(x, y, x + width, y + height), scaleFactor,
                bitmapRequestHandler, bitmapRequestHandler);
    }

    /**
     * Remove previously fetched bitmaps that are no longer required according to
     * {@link #mRequiredBitmaps}.
     */
    private void deleteUnrequiredBitmaps() {
        for (int row = 0; row < mBitmapMatrix.length; row++) {
            for (int col = 0; col < mBitmapMatrix[row].length; col++) {
                Bitmap bitmap = mBitmapMatrix[row][col];
                if (!mRequiredBitmaps[row][col] && bitmap != null) {
                    bitmap.recycle();
                    mBitmapMatrix[row][col] = null;
                }
            }
        }
    }

    /**
     * Used as the callback for bitmap requests from the Paint Preview compositor.
     */
    private class BitmapRequestHandler implements Callback<Bitmap>, Runnable {
        int mRequestRow;
        int mRequestCol;
        float mRequestScaleFactor;

        private BitmapRequestHandler(int requestRow, int requestCol, float requestScaleFactor) {
            mRequestRow = requestRow;
            mRequestCol = requestCol;
            mRequestScaleFactor = requestScaleFactor;
        }

        /**
         * Called when bitmap is successfully composited.
         * @param result
         */
        @Override
        public void onResult(Bitmap result) {
            if (result == null) {
                run();
                return;
            }
            if (mViewport.getScale() != mRequestScaleFactor
                    || !mPendingBitmapRequests[mRequestRow][mRequestCol]
                    || !mRequiredBitmaps[mRequestRow][mRequestCol]) {
                result.recycle();
                deleteUnrequiredBitmaps();
                return;
            }

            mPendingBitmapRequests[mRequestRow][mRequestCol] = false;
            mBitmapMatrix[mRequestRow][mRequestCol] = result;
            mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix);
            deleteUnrequiredBitmaps();
        }

        /**
         * Called when there was an error compositing the bitmap.
         */
        @Override
        public void run() {
            if (mViewport.getScale() != mRequestScaleFactor) return;

            // TODO(crbug.com/1021590): Handle errors.
            assert mBitmapMatrix != null;
            assert mBitmapMatrix[mRequestRow][mRequestCol] == null;
            assert mPendingBitmapRequests[mRequestRow][mRequestCol];

            mPendingBitmapRequests[mRequestRow][mRequestCol] = false;
        }
    }
}
