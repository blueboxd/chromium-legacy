// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Handler;
import android.view.View;
import android.widget.OverScroller;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.OverscrollHandler;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    /** The content width inside this frame, at a scale factor of 1. */
    private final int mContentWidth;
    /** The content height inside this frame, at a scale factor of 1. */
    private final int mContentHeight;
    /**
     * Contains all {@link View}s corresponding to this frame's sub-frames.
     */
    private final List<View> mSubFrameViews = new ArrayList<>();
    /**
     * Contains all clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameRects = new ArrayList<>();
    /**
     * Contains scaled clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameScaledRects = new ArrayList<>();
    /**
     * Contains views for currently visible sub-frames according to {@link #mViewPort}.
     */
    private final List<View> mVisibleSubFrameViews = new ArrayList<>();
    /**
     * Contains scaled clip rects for currently visible sub-frames according to {@link #mViewPort}.
     */
    private final List<Rect> mVisibleSubFrameScaledRects = new ArrayList<>();
    private final PropertyModel mModel;
    private final PlayerCompositorDelegate mCompositorDelegate;
    private final OverScroller mScroller;
    private final Handler mScrollerHandler;
    /** The user-visible area for this frame. */
    private final Rect mViewportRect = new Rect();
    /** Rect used for requesting a new bitmap from Paint Preview compositor. */
    private final Rect mBitmapRequestRect = new Rect();
    /** Dimension of tiles for each scale factor. */
    private final Map<Float, int[]> mTileDimensions = new HashMap<>();
    /**
     * A scale factor cache of matrices of bitmaps that make up the content of this frame at a
     * given scale factor.
     */
    private final Map<Float, Bitmap[][]> mBitmapMatrix = new HashMap<>();
    /** Whether a request for a bitmap tile is pending, mapped by scale factor. */
    private final Map<Float, boolean[][]> mPendingBitmapRequests = new HashMap<>();
    /**
     * Whether we currently need a bitmap tile. This is used for deleting bitmaps that we don't
     * need and freeing up memory.
     */
    @VisibleForTesting
    final Map<Float, boolean[][]> mRequiredBitmaps = new HashMap<>();
    /** The current scale factor. */
    private float mScaleFactor;
    private float mInitialScaleFactor;
    private float mUncommittedScaleFactor = 0f;
    @VisibleForTesting
    final Matrix mViewportScaleMatrix = new Matrix();
    private final Matrix mBitmapScaleMatrix = new Matrix();

    /** For swipe-to-refresh logic */
    private OverscrollHandler mOverscrollHandler;
    private boolean mIsOverscrolling = false;
    private float mOverscrollAmount = 0f;

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            OverScroller scroller, UnguessableToken frameGuid, int contentWidth, int contentHeight,
            int initialScrollX, int initialScrollY) {
        mModel = model;
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, mVisibleSubFrameViews);
        mModel.set(PlayerFrameProperties.SUBFRAME_RECTS, mVisibleSubFrameScaledRects);
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);

        mCompositorDelegate = compositorDelegate;
        mScroller = scroller;
        mGuid = frameGuid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
        mScrollerHandler = new Handler();
        mViewportRect.offset(initialScrollX, initialScrollY);
        mViewportScaleMatrix.postTranslate(-initialScrollX, -initialScrollY);
    }

    /**
     * Adds a new sub-frame to this frame.
     * @param subFrameView The {@link View} associated with the sub-frame.
     * @param clipRect     The bounds of the sub-frame, relative to this frame.
     */
    void addSubFrame(View subFrameView, Rect clipRect) {
        mSubFrameViews.add(subFrameView);
        mSubFrameRects.add(clipRect);
        mSubFrameScaledRects.add(new Rect());
    }

    @Override
    public void setLayoutDimensions(int width, int height) {
        // Don't trigger a re-draw if we are actively scaling.
        if (!mBitmapScaleMatrix.isIdentity()) {
            mViewportRect.set(mViewportRect.left, mViewportRect.top, mViewportRect.left + width,
                    mViewportRect.top + height);
            // Set scale factor to 0 so subframes get the correct scale factor on scale completion.
            mScaleFactor = 0;
            return;
        }

        // Set initial scale so that content width fits within the layout dimensions.
        mInitialScaleFactor = ((float) width) / ((float) mContentWidth);
        updateViewportSize(
                width, height, (mScaleFactor == 0f) ? mInitialScaleFactor : mScaleFactor);
    }

    @Override
    public void setBitmapScaleMatrix(Matrix matrix, float scaleFactor) {
        // Don't update the subframes if the matrix is identity as it will be forcibly recalculated.
        if (!matrix.isIdentity()) {
            updateSubFrames(mViewportRect, scaleFactor);
        }
        setBitmapScaleMatrixInternal(matrix, scaleFactor);
    }

    private void setBitmapScaleMatrixInternal(Matrix matrix, float scaleFactor) {
        float[] matrixValues = new float[9];
        matrix.getValues(matrixValues);
        mBitmapScaleMatrix.setValues(matrixValues);
        Matrix childBitmapScaleMatrix = new Matrix();
        childBitmapScaleMatrix.setScale(
                matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
        for (View subFrameView : mVisibleSubFrameViews) {
            ((PlayerFrameView) subFrameView)
                    .updateDelegateScaleMatrix(childBitmapScaleMatrix, scaleFactor);
        }
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);
    }

    @Override
    public void forceRedraw() {
        mInitialScaleFactor = ((float) mViewportRect.width()) / ((float) mContentWidth);
        moveViewport(0, 0, (mScaleFactor == 0f) ? mInitialScaleFactor : mScaleFactor);
        for (View subFrameView : mVisibleSubFrameViews) {
            ((PlayerFrameView) subFrameView).forceRedraw();
        }
    }

    void updateViewportSize(int width, int height, float scaleFactor) {
        if (width <= 0 || height <= 0) return;

        // Ensure the viewport is within the bounds of the content.
        final int left = Math.max(
                0, Math.min(mViewportRect.left, Math.round(mContentWidth * scaleFactor) - width));
        final int top = Math.max(
                0, Math.min(mViewportRect.top, Math.round(mContentHeight * scaleFactor) - height));
        mViewportRect.set(left, top, left + width, top + height);
        moveViewport(0, 0, scaleFactor);
    }

    /**
     * Called when the view port is moved or the scale factor is changed. Updates the view port
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param distanceX   The horizontal distance that the view port should be moved by.
     * @param distanceY   The vertical distance that the view port should be moved by.
     * @param scaleFactor The new scale factor.
     */
    private void moveViewport(int distanceX, int distanceY, float scaleFactor) {
        // Initialize the bitmap matrix for this scale factor if we haven't already.
        int[] tileDimensions = mTileDimensions.get(scaleFactor);
        Bitmap[][] bitmapMatrix = mBitmapMatrix.get(scaleFactor);
        boolean[][] pendingBitmapRequests = mPendingBitmapRequests.get(scaleFactor);
        boolean[][] requiredBitmaps = mRequiredBitmaps.get(scaleFactor);
        if (bitmapMatrix == null) {
            // Each tile is as big as the initial view port. Here we determine the number of
            // columns and rows for the current scale factor.
            int rows = (int) Math.ceil((mContentHeight * scaleFactor) / mViewportRect.height());
            int cols = (int) Math.ceil((mContentWidth * scaleFactor) / mViewportRect.width());
            tileDimensions = new int[] {mViewportRect.width(), mViewportRect.height()};
            bitmapMatrix = new Bitmap[rows][cols];
            pendingBitmapRequests = new boolean[rows][cols];
            requiredBitmaps = new boolean[rows][cols];
            mTileDimensions.put(scaleFactor, tileDimensions);
            mBitmapMatrix.put(scaleFactor, bitmapMatrix);
            mPendingBitmapRequests.put(scaleFactor, pendingBitmapRequests);
            mRequiredBitmaps.put(scaleFactor, requiredBitmaps);
        }

        // Update mViewportRect and let the view know. PropertyModelChangeProcessor is smart about
        // this and will only update the view if mViewportRect is actually changed.
        mViewportRect.offset(distanceX, distanceY);
        // Keep translations up to date for the viewport matrix so that future scale operations are
        // correct.
        mViewportScaleMatrix.postTranslate(-distanceX, -distanceY);
        updateSubFrames(mViewportRect, scaleFactor);
        mModel.set(PlayerFrameProperties.TILE_DIMENSIONS, tileDimensions);
        mModel.set(PlayerFrameProperties.VIEWPORT, mViewportRect);

        // Clear the required bitmaps matrix. It will be updated in #requestBitmapForTile.
        for (int row = 0; row < requiredBitmaps.length; row++) {
            for (int col = 0; col < requiredBitmaps[row].length; col++) {
                requiredBitmaps[row][col] = false;
            }
        }

        // Request bitmaps for tiles inside the view port that don't already have a bitmap.
        final int tileWidth = tileDimensions[0];
        final int tileHeight = tileDimensions[1];
        final int colStart = Math.max(0, (int) Math.floor((double) mViewportRect.left / tileWidth));
        final int colEnd = Math.min(requiredBitmaps[0].length,
                (int) Math.ceil((double) mViewportRect.right / tileWidth));
        final int rowStart = Math.max(0, (int) Math.floor((double) mViewportRect.top / tileHeight));
        final int rowEnd = Math.min(requiredBitmaps.length,
                (int) Math.ceil((double) mViewportRect.bottom / tileHeight));
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
        if (scaleFactor != mScaleFactor) {
            mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix.get(scaleFactor));
            mBitmapMatrix.remove(mScaleFactor); // Eagerly free to avoid OOM.
            mRequiredBitmaps.remove(mScaleFactor);
            mScaleFactor = scaleFactor;
        }
    }

    private void updateSubFrames(Rect viewport, float scaleFactor) {
        mVisibleSubFrameViews.clear();
        mVisibleSubFrameScaledRects.clear();
        for (int i = 0; i < mSubFrameRects.size(); i++) {
            Rect subFrameScaledRect = mSubFrameScaledRects.get(i);
            scaleRect(mSubFrameRects.get(i), subFrameScaledRect, scaleFactor);
            if (Rect.intersects(subFrameScaledRect, viewport)) {
                int transformedLeft = subFrameScaledRect.left - viewport.left;
                int transformedTop = subFrameScaledRect.top - viewport.top;
                subFrameScaledRect.set(transformedLeft, transformedTop,
                        transformedLeft + subFrameScaledRect.width(),
                        transformedTop + subFrameScaledRect.height());
                mVisibleSubFrameViews.add(mSubFrameViews.get(i));
                mVisibleSubFrameScaledRects.add(subFrameScaledRect);
            }
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
        Bitmap[][] bitmapMatrix = mBitmapMatrix.get(scaleFactor);
        if (bitmapMatrix == null) return;

        if (row > 0) {
            requestBitmapForTile(col * tileWidth, (row - 1) * tileHeight, tileWidth, tileHeight,
                    row - 1, col, scaleFactor);
        }
        if (row < bitmapMatrix.length - 1) {
            requestBitmapForTile(col * tileWidth, (row + 1) * tileHeight, tileWidth, tileHeight,
                    row + 1, col, scaleFactor);
        }
        if (col > 0) {
            requestBitmapForTile((col - 1) * tileWidth, row * tileHeight, tileWidth, tileHeight,
                    row, col - 1, scaleFactor);
        }
        if (col < bitmapMatrix[row].length - 1) {
            requestBitmapForTile((col + 1) * tileWidth, row * tileHeight, tileWidth, tileHeight,
                    row, col + 1, scaleFactor);
        }
    }

    private void requestBitmapForTile(
            int x, int y, int width, int height, int row, int col, float scaleFactor) {
        Bitmap[][] bitmapMatrix = mBitmapMatrix.get(scaleFactor);
        boolean[][] pendingBitmapRequests = mPendingBitmapRequests.get(scaleFactor);
        boolean[][] requiredBitmaps = mRequiredBitmaps.get(scaleFactor);
        if (requiredBitmaps == null) return;

        requiredBitmaps[row][col] = true;
        if (bitmapMatrix == null || pendingBitmapRequests == null || bitmapMatrix[row][col] != null
                || pendingBitmapRequests[row][col]) {
            return;
        }

        mBitmapRequestRect.set(x, y, x + width, y + height);
        BitmapRequestHandler bitmapRequestHandler = new BitmapRequestHandler(row, col, scaleFactor);
        pendingBitmapRequests[row][col] = true;
        mCompositorDelegate.requestBitmap(
                mGuid, mBitmapRequestRect, scaleFactor, bitmapRequestHandler, bitmapRequestHandler);
    }

    /**
     * Remove previously fetched bitmaps that are no longer required according to
     * {@link #mRequiredBitmaps}.
     */
    private void deleteUnrequiredBitmaps(float scaleFactor) {
        Bitmap[][] bitmapMatrix = mBitmapMatrix.get(scaleFactor);
        boolean[][] requiredBitmaps = mRequiredBitmaps.get(scaleFactor);
        if (bitmapMatrix == null || requiredBitmaps == null) return;

        for (int row = 0; row < bitmapMatrix.length; row++) {
            for (int col = 0; col < bitmapMatrix[row].length; col++) {
                Bitmap bitmap = bitmapMatrix[row][col];
                if (!requiredBitmaps[row][col] && bitmap != null) {
                    bitmap.recycle();
                    bitmapMatrix[row][col] = null;
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
        if (mOverscrollHandler == null || mViewportRect.top != 0) return false;

        // Ignore if there is no active overscroll and the direction is down.
        if (!mIsOverscrolling && distanceY <= 0) return false;

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
        float scaledContentWidth = mContentWidth * mScaleFactor;
        float scaledContentHeight = mContentHeight * mScaleFactor;

        if (mViewportRect.left > 0 && distanceX < 0) {
            validDistanceX = (int) Math.max(distanceX, -1f * mViewportRect.left);
        } else if (mViewportRect.right < scaledContentWidth && distanceX > 0) {
            validDistanceX = (int) Math.min(distanceX, scaledContentWidth - mViewportRect.right);
        }
        if (mViewportRect.top > 0 && distanceY < 0) {
            validDistanceY = (int) Math.max(distanceY, -1f * mViewportRect.top);
        } else if (mViewportRect.bottom < scaledContentHeight && distanceY > 0) {
            validDistanceY = (int) Math.min(distanceY, scaledContentHeight - mViewportRect.bottom);
        }

        if (validDistanceX == 0 && validDistanceY == 0) {
            return false;
        }

        moveViewport(validDistanceX, validDistanceY, mScaleFactor);
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
     * accumulates the scale starting from the currently committed |mScaleFactor|. This is
     * committed when {@link #scaleFinished()} event occurs. This is for the viewport reference
     * frame. |mViewportScaleMatrix| also accumulates the transforms to track the translation
     * behavior.
     *
     * |mBitmapScaleMatrix| tracks scaling from the perspective of the bitmaps. This is used to
     * transform the canvas the bitmaps are painted on such that scaled images can be shown
     * mid-gesture.
     *
     * Each subframe is updated with a new rect based on the interim scale factor and when the
     * matrix is set in {@link PlayerFrameView#updateMatrix} the subframe matricies are recursively
     * updated.
     *
     * On {@link #scaleFinished()} the gesture is now considered finished.
     *
     * The values of |mViewportScaleMatrix| are extracted to get the final translation applied to
     * the viewport. The transform for the bitmaps (that is |mBitmapScaleMatrix|) is cancelled.
     *
     * During {@link #moveViewport()} new bitmaps are requested for the main frame and subframes
     * to improve quality.
     *
     * NOTE: |mViewportScaleMatrix| also need to consume scroll events in order to keep track of
     * the correct translation.
     */
    @Override
    public boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        // This is filtered to only apply to the top level view upstream.
        if (mUncommittedScaleFactor == 0f) {
            mUncommittedScaleFactor = mScaleFactor;
        }
        mUncommittedScaleFactor *= scaleFactor;

        // Don't scale outside of the acceptable range. The value is still accumulated such that the
        // continuous gesture feels smooth.
        if (mUncommittedScaleFactor < mInitialScaleFactor) return true;
        if (mUncommittedScaleFactor > MAX_SCALE_FACTOR) return true;

        // TODO(crbug/1090804): trigger a fetch of new bitmaps periodically when zooming out.

        mViewportScaleMatrix.postScale(scaleFactor, scaleFactor, focalPointX, focalPointY);
        mBitmapScaleMatrix.postScale(scaleFactor, scaleFactor, focalPointX, focalPointY);

        float[] viewportScaleMatrixValues = new float[9];
        mViewportScaleMatrix.getValues(viewportScaleMatrixValues);

        float[] bitmapScaleMatrixValues = new float[9];
        mBitmapScaleMatrix.getValues(bitmapScaleMatrixValues);

        // It is possible the scale pushed the viewport outside the content bounds. These new values
        // are forced to be within bounds.
        final float newX = -1f
                * Math.max(0f,
                        Math.min(-viewportScaleMatrixValues[Matrix.MTRANS_X],
                                mContentWidth * mUncommittedScaleFactor - mViewportRect.width()));
        final float newY = -1f
                * Math.max(0f,
                        Math.min(-viewportScaleMatrixValues[Matrix.MTRANS_Y],
                                mContentHeight * mUncommittedScaleFactor - mViewportRect.height()));
        final int newXRounded = Math.abs(Math.round(newX));
        final int newYRounded = Math.abs(Math.round(newY));
        updateSubFrames(new Rect(newXRounded, newYRounded, newXRounded + mViewportRect.width(),
                                newYRounded + mViewportRect.height()),
                mUncommittedScaleFactor);

        if (newX != viewportScaleMatrixValues[Matrix.MTRANS_X]
                || newY != viewportScaleMatrixValues[Matrix.MTRANS_Y]) {
            // This is the delta required to force the viewport to be inside the bounds of the
            // content.
            final float deltaX = newX - viewportScaleMatrixValues[Matrix.MTRANS_X];
            final float deltaY = newY - viewportScaleMatrixValues[Matrix.MTRANS_Y];

            // Directly used the forced bounds of the viewport reference frame for the viewport
            // scale matrix.
            viewportScaleMatrixValues[Matrix.MTRANS_X] = newX;
            viewportScaleMatrixValues[Matrix.MTRANS_Y] = newY;
            mViewportScaleMatrix.setValues(viewportScaleMatrixValues);

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

        float[] matrixValues = new float[9];
        mViewportScaleMatrix.getValues(matrixValues);
        final float newX = Math.max(0f,
                Math.min(-matrixValues[Matrix.MTRANS_X],
                        mContentWidth * finalScaleFactor - mViewportRect.width()));
        final float newY = Math.max(0f,
                Math.min(-matrixValues[Matrix.MTRANS_Y],
                        mContentHeight * finalScaleFactor - mViewportRect.height()));
        matrixValues[Matrix.MTRANS_X] = -newX;
        matrixValues[Matrix.MTRANS_Y] = -newY;
        matrixValues[Matrix.MSCALE_X] = finalScaleFactor;
        matrixValues[Matrix.MSCALE_Y] = finalScaleFactor;
        mViewportScaleMatrix.setValues(matrixValues);
        final int newXRounded = Math.round(newX);
        final int newYRounded = Math.round(newY);
        mViewportRect.set(newXRounded, newYRounded, newXRounded + mViewportRect.width(),
                newYRounded + mViewportRect.height());

        moveViewport(0, 0, finalScaleFactor);
        for (View subFrameView : mVisibleSubFrameViews) {
            ((PlayerFrameView) subFrameView).forceRedraw();
        }

        return true;
    }

    @Override
    public void onClick(int x, int y) {
        // x and y are in the View's coordinate system (scaled). This needs to be adjusted to the
        // absolute coordinate system for hit testing.
        mCompositorDelegate.onClick(mGuid,
                Math.round((float) (mViewportRect.left + x) / mScaleFactor),
                Math.round((float) (mViewportRect.top + y) / mScaleFactor));
    }

    @Override
    public boolean onFling(float velocityX, float velocityY) {
        int scaledContentWidth = (int) (mContentWidth * mScaleFactor);
        int scaledContentHeight = (int) (mContentHeight * mScaleFactor);
        mScroller.forceFinished(true);
        mScroller.fling(mViewportRect.left, mViewportRect.top, (int) -velocityX, (int) -velocityY,
                0, scaledContentWidth - mViewportRect.width(), 0,
                scaledContentHeight - mViewportRect.height());

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
        int deltaX = mScroller.getCurrX() - mViewportRect.left;
        int deltaY = mScroller.getCurrY() - mViewportRect.top;
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
            Bitmap[][] bitmapMatrix = mBitmapMatrix.get(mRequestScaleFactor);
            boolean[][] requiredBitmaps = mRequiredBitmaps.get(mRequestScaleFactor);
            if (bitmapMatrix == null || requiredBitmaps == null) {
                result.recycle();
                return;
            }

            assert mBitmapMatrix.get(mRequestScaleFactor)[mRequestRow][mRequestCol] == null;
            assert mPendingBitmapRequests.get(mRequestScaleFactor)[mRequestRow][mRequestCol];

            mPendingBitmapRequests.get(mScaleFactor)[mRequestRow][mRequestCol] = false;
            if (requiredBitmaps[mRequestRow][mRequestCol]) {
                bitmapMatrix[mRequestRow][mRequestCol] = result;
                if (PlayerFrameMediator.this.mScaleFactor == mRequestScaleFactor) {
                    mModel.set(PlayerFrameProperties.BITMAP_MATRIX, bitmapMatrix);
                }
            } else {
                result.recycle();
            }
            deleteUnrequiredBitmaps(mRequestScaleFactor);
        }

        /**
         * Called when there was an error compositing the bitmap.
         */
        @Override
        public void run() {
            // TODO(crbug.com/1021590): Handle errors.
            assert mBitmapMatrix.get(mRequestScaleFactor) != null;
            assert mBitmapMatrix.get(mRequestScaleFactor)[mRequestRow][mRequestCol] == null;
            assert mPendingBitmapRequests.get(mRequestScaleFactor)[mRequestRow][mRequestCol];

            mPendingBitmapRequests.get(mScaleFactor)[mRequestRow][mRequestCol] = false;
        }
    }
}
