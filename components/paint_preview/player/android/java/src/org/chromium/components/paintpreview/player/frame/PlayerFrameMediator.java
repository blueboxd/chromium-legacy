// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Handler;
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
class PlayerFrameMediator implements PlayerFrameViewDelegate {
    private static final float MAX_SCALE_FACTOR = 5f;

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
    private final OverScroller mScroller;
    private final Handler mScrollerHandler;
    /** The viewport of this frame. */
    @VisibleForTesting
    final PlayerFrameViewport mViewport = new PlayerFrameViewport(new Size(0, 0), new Matrix());
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
    private float mInitialScaleFactor;
    private float mUncommittedScaleFactor = 0f;

    /** For swipe-to-refresh logic */
    private OverscrollHandler mOverscrollHandler;
    private boolean mIsOverscrolling = false;
    private float mOverscrollAmount = 0f;

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            OverScroller scroller, UnguessableToken frameGuid, int contentWidth, int contentHeight,
            int initialScrollX, int initialScrollY) {
        mModel = model;
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);

        mCompositorDelegate = compositorDelegate;
        mScroller = scroller;
        mGuid = frameGuid;
        mContentSize = new Size(contentWidth, contentHeight);
        mScrollerHandler = new Handler();
        mViewport.offset(initialScrollX, initialScrollY);
        mViewport.setScale(0f);
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

    @Override
    public void setLayoutDimensions(int width, int height) {
        // Don't trigger a re-draw if we are actively scaling.
        if (!mBitmapScaleMatrix.isIdentity()) {
            mViewport.setSize(width, height);
            return;
        }

        // Set initial scale so that content width fits within the layout dimensions.
        mInitialScaleFactor = ((float) width) / ((float) mContentSize.getWidth());
        final float scaleFactor = mViewport.getScale();
        updateViewportSize(width, height, (scaleFactor == 0f) ? mInitialScaleFactor : scaleFactor);
    }

    public void setBitmapScaleMatrix(Matrix matrix, float scaleFactor) {
        // Don't update the subframes if the matrix is identity as it will be forcibly recalculated.
        if (!matrix.isIdentity()) {
            updateSubFrames(mViewport.asRect(), scaleFactor);
        }
        setBitmapScaleMatrixInternal(matrix, scaleFactor);
    }

    /**
     * Resets the scale factor post scale.
     */
    @VisibleForTesting
    void resetScaleFactor() {
        // Set scale factor to 0 so subframes get the correct scale factor on scale completion.
        mViewport.setScale(0f);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).resetScaleFactor();
        }
    }

    private void setBitmapScaleMatrixInternal(Matrix matrix, float scaleFactor) {
        float[] matrixValues = new float[9];
        matrix.getValues(matrixValues);
        mBitmapScaleMatrix.setValues(matrixValues);
        Matrix childBitmapScaleMatrix = new Matrix();
        childBitmapScaleMatrix.setScale(
                matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).setBitmapScaleMatrix(childBitmapScaleMatrix, scaleFactor);
        }
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);
    }

    @VisibleForTesting
    void forceRedraw() {
        mInitialScaleFactor = ((float) mViewport.getWidth()) / ((float) mContentSize.getWidth());
        final float scaleFactor = mViewport.getScale();
        mViewport.setScale((scaleFactor == 0f) ? mInitialScaleFactor : scaleFactor);
        moveViewport(0, 0, true);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }
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
        moveViewport(0, 0, oldScaleFactor != scaleFactor);
    }

    /**
     * Called when the view port is moved or the scale factor is changed. Updates the view port
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param distanceX    The horizontal distance that the view port should be moved by.
     * @param distanceY    The vertical distance that the view port should be moved by.
     * @param scaleUpdated Whether the scale was updated.
     */
    private void moveViewport(int distanceX, int distanceY, boolean scaleUpdated) {
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

        // Update mViewport and let the view know. PropertyModelChangeProcessor is smart about
        // this and will only update the view if mViewport's rect is actually changed.
        mViewport.offset(distanceX, distanceY);
        Rect viewportRect = mViewport.asRect();
        updateSubFrames(viewportRect, mViewport.getScale());
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

    private void updateSubFrames(Rect viewport, float scaleFactor) {
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
     * Called on scroll events from the user. Checks if scrolling is possible, and if so, calls
     * {@link #moveViewport}.
     * @param distanceX Horizontal scroll distance in pixels.
     * @param distanceY Vertical scroll distance in pixels.
     * @return Whether the scrolling was possible and view port was updated.
     */
    @Override
    public boolean scrollBy(float distanceX, float distanceY) {
        mScroller.forceFinished(true);

        return scrollByInternal(distanceX, distanceY);
    }

    @Override
    public void onRelease() {
        if (mOverscrollHandler == null || !mIsOverscrolling) return;

        mOverscrollHandler.release();
        mIsOverscrolling = false;
        mOverscrollAmount = 0.0f;
    }

    private boolean maybeHandleOverscroll(float distanceY) {
        if (mOverscrollHandler == null || mViewport.getTransY() != 0f) return false;

        // Ignore if there is no active overscroll and the direction is down.
        if (!mIsOverscrolling && distanceY <= 0) return false;

        // TODO(crbug/1100338): Propagate this state to child mediators to
        // support easing.
        mOverscrollAmount += distanceY;

        // If the overscroll is completely eased off the cancel the event.
        if (mOverscrollAmount <= 0) {
            mIsOverscrolling = false;
            mOverscrollHandler.reset();
            return false;
        }

        // Start the overscroll event if the scroll direction is correct and one isn't active.
        if (!mIsOverscrolling && distanceY > 0) {
            mOverscrollAmount = distanceY;
            mIsOverscrolling = mOverscrollHandler.start();
        }
        mOverscrollHandler.pull(distanceY);
        return mIsOverscrolling;
    }

    private boolean scrollByInternal(float distanceX, float distanceY) {
        if (maybeHandleOverscroll(-distanceY)) return true;

        int validDistanceX = 0;
        int validDistanceY = 0;
        final float scaleFactor = mViewport.getScale();
        float scaledContentWidth = mContentSize.getWidth() * scaleFactor;
        float scaledContentHeight = mContentSize.getHeight() * scaleFactor;

        Rect viewportRect = mViewport.asRect();
        if (viewportRect.left > 0 && distanceX < 0) {
            validDistanceX = (int) Math.max(distanceX, -1f * viewportRect.left);
        } else if (viewportRect.right < scaledContentWidth && distanceX > 0) {
            validDistanceX = (int) Math.min(distanceX, scaledContentWidth - viewportRect.right);
        }
        if (viewportRect.top > 0 && distanceY < 0) {
            validDistanceY = (int) Math.max(distanceY, -1f * viewportRect.top);
        } else if (viewportRect.bottom < scaledContentHeight && distanceY > 0) {
            validDistanceY = (int) Math.min(distanceY, scaledContentHeight - viewportRect.bottom);
        }

        if (validDistanceX == 0 && validDistanceY == 0) {
            return false;
        }

        moveViewport(validDistanceX, validDistanceY, false);
        return true;
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
     * During {@link #moveViewport()} new bitmaps are requested for the main frame and subframes
     * to improve quality.
     */
    @Override
    public boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        // This is filtered to only apply to the top level view upstream.
        if (mUncommittedScaleFactor == 0f) {
            mUncommittedScaleFactor = mViewport.getScale();
        }
        mUncommittedScaleFactor *= scaleFactor;

        // Don't scale outside of the acceptable range. The value is still accumulated such that the
        // continuous gesture feels smooth.
        if (mUncommittedScaleFactor < mInitialScaleFactor) return true;
        if (mUncommittedScaleFactor > MAX_SCALE_FACTOR) return true;

        // TODO(crbug/1090804): trigger a fetch of new bitmaps periodically when zooming out.

        mViewport.scale(scaleFactor, focalPointX, focalPointY);
        mBitmapScaleMatrix.postScale(scaleFactor, scaleFactor, focalPointX, focalPointY);

        float[] bitmapScaleMatrixValues = new float[9];
        mBitmapScaleMatrix.getValues(bitmapScaleMatrixValues);

        // It is possible the scale pushed the viewport outside the content bounds. These new values
        // are forced to be within bounds.
        Rect uncorrectedViewportRect = mViewport.asRect();
        final float correctedX = Math.max(0f,
                Math.min(uncorrectedViewportRect.left,
                        mContentSize.getWidth() * mUncommittedScaleFactor - mViewport.getWidth()));
        final float correctedY = Math.max(0f,
                Math.min(uncorrectedViewportRect.top,
                        mContentSize.getHeight() * mUncommittedScaleFactor
                                - mViewport.getHeight()));
        final int correctedXRounded = Math.abs(Math.round(correctedX));
        final int correctedYRounded = Math.abs(Math.round(correctedY));
        updateSubFrames(new Rect(correctedXRounded, correctedYRounded,
                                correctedXRounded + mViewport.getWidth(),
                                correctedYRounded + mViewport.getHeight()),
                mUncommittedScaleFactor);

        if (correctedX != uncorrectedViewportRect.left
                || correctedY != uncorrectedViewportRect.top) {
            // This is the delta required to force the viewport to be inside the bounds of the
            // content.
            final float deltaX = uncorrectedViewportRect.left - correctedX;
            final float deltaY = uncorrectedViewportRect.top - correctedY;

            // Directly used the forced bounds of the viewport reference frame for the viewport
            // scale matrix.
            mViewport.setTrans(correctedX, correctedY);

            // For the bitmap matrix we only want the delta as its position will be different as the
            // coordinates are bitmap relative.
            bitmapScaleMatrixValues[Matrix.MTRANS_X] += deltaX;
            bitmapScaleMatrixValues[Matrix.MTRANS_Y] += deltaY;
            mBitmapScaleMatrix.setValues(bitmapScaleMatrixValues);
        }
        setBitmapScaleMatrixInternal(mBitmapScaleMatrix, mUncommittedScaleFactor);

        return true;
    }

    @Override
    public boolean scaleFinished(float scaleFactor, float focalPointX, float focalPointY) {
        // Remove the bitmap scaling to avoid issues when new bitmaps are requested.
        // TODO(crbug/1090804): Defer clearing this so that double buffering can occur.
        mBitmapScaleMatrix.reset();
        setBitmapScaleMatrixInternal(mBitmapScaleMatrix, 1f);

        final float finalScaleFactor =
                Math.max(mInitialScaleFactor, Math.min(mUncommittedScaleFactor, MAX_SCALE_FACTOR));
        mUncommittedScaleFactor = 0f;

        final float correctedX = Math.max(0f,
                Math.min(mViewport.getTransX(),
                        mContentSize.getWidth() * finalScaleFactor - mViewport.getWidth()));
        final float correctedY = Math.max(0f,
                Math.min(mViewport.getTransY(),
                        mContentSize.getHeight() * finalScaleFactor - mViewport.getHeight()));
        mViewport.setTrans(correctedX, correctedY);
        mViewport.setScale(finalScaleFactor);

        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).resetScaleFactor();
        }
        moveViewport(0, 0, true);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }

        return true;
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

    @Override
    public boolean onFling(float velocityX, float velocityY) {
        final float scaleFactor = mViewport.getScale();
        int scaledContentWidth = (int) (mContentSize.getWidth() * scaleFactor);
        int scaledContentHeight = (int) (mContentSize.getHeight() * scaleFactor);
        mScroller.forceFinished(true);
        Rect viewportRect = mViewport.asRect();
        mScroller.fling(viewportRect.left, viewportRect.top, (int) -velocityX, (int) -velocityY, 0,
                scaledContentWidth - viewportRect.width(), 0,
                scaledContentHeight - viewportRect.height());

        mScrollerHandler.post(this::handleFling);
        return true;
    }

    public void setOverscrollHandler(OverscrollHandler overscrollHandler) {
        mOverscrollHandler = overscrollHandler;
    }

    /**
     * Handles a fling update by computing the next scroll offset and programmatically scrolling.
     */
    private void handleFling() {
        if (mScroller.isFinished()) return;

        boolean shouldContinue = mScroller.computeScrollOffset();
        int deltaX = mScroller.getCurrX() - Math.round(mViewport.getTransX());
        int deltaY = mScroller.getCurrY() - Math.round(mViewport.getTransY());
        scrollByInternal(deltaX, deltaY);

        if (shouldContinue) {
            mScrollerHandler.post(this::handleFling);
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
