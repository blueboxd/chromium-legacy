// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.util.Pair;
import android.view.View;
import android.widget.Scroller;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
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
    /** The GUID associated with the frame that this class is representing. */
    private final UnguessableToken mGuid;
    /** The content width inside this frame, at a scale factor of 1. */
    private final int mContentWidth;
    /** The content height inside this frame, at a scale factor of 1. */
    private final int mContentHeight;
    /**
     * A list of {@link PlayerFrameCoordinator}s and {@link Rect}s representing this frame's
     * sub-frames and their coordinates.
     */
    private final List<Pair<View, Rect>> mSubFrames = new ArrayList<>();
    private final PropertyModel mModel;
    private final PlayerCompositorDelegate mCompositorDelegate;
    private final Scroller mScroller;
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

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            Scroller scroller, UnguessableToken frameGuid, int contentWidth, int contentHeight) {
        mModel = model;
        mCompositorDelegate = compositorDelegate;
        mScroller = scroller;
        mGuid = frameGuid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
        mScrollerHandler = new Handler();
    }

    /**
     * Adds a new sub-frame to this frame.
     * @param subFrameView The {@link View} associated with the sub-frame.
     * @param clipRect     The bounds of the sub-frame, relative to this frame.
     */
    void addSubFrame(View subFrameView, Rect clipRect) {
        mSubFrames.add(new Pair<>(subFrameView, clipRect));
    }

    @Override
    public void setLayoutDimensions(int width, int height) {
        // Set initial scale so that content width fits within the layout dimensions.
        float initialScaleFactor = ((float) width) / ((float) mContentWidth);
        updateViewportSize(width, height, mScaleFactor == 0f ? initialScaleFactor : mScaleFactor);
    }

    void updateViewportSize(int width, int height, float scaleFactor) {
        if (width <= 0 || height <= 0) return;

        mViewportRect.set(mViewportRect.left, mViewportRect.top, mViewportRect.left + width,
                mViewportRect.top + height);
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

        // If the scale factor is changed, the view should get the correct bitmap matrix.
        if (scaleFactor != mScaleFactor) {
            mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix.get(scaleFactor));
            mScaleFactor = scaleFactor;
        }

        // Update mViewportRect and let the view know. PropertyModelChangeProcessor is smart about
        // this and will only update the view if mViewportRect is actually changed.
        mViewportRect.offset(distanceX, distanceY);
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
        final int colStart = mViewportRect.left / tileWidth;
        final int colEnd = (int) Math.ceil((double) mViewportRect.right / tileWidth);
        final int rowStart = mViewportRect.top / tileHeight;
        final int rowEnd = (int) Math.ceil((double) mViewportRect.bottom / tileHeight);
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

        // Add visible sub-frames to the view.
        List<Pair<View, Rect>> visibleSubFrames = new ArrayList<>();
        for (int i = 0; i < mSubFrames.size(); i++) {
            // TODO(crbug.com/1020702): These values should be scaled for scale factors other than
            // 1.
            if (Rect.intersects(mSubFrames.get(i).second, mViewportRect)) {
                visibleSubFrames.add(mSubFrames.get(i));
            }
        }
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, visibleSubFrames);
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

    private boolean scrollByInternal(float distanceX, float distanceY) {
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

        if (validDistanceX == 0 && validDistanceY == 0) return false;

        moveViewport(validDistanceX, validDistanceY, mScaleFactor);
        return true;
    }

    @Override
    public boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY) {
        // TODO(crbug.com/1020702): Add support for zooming.
        return false;
    }

    @Override
    public void onClick(int x, int y) {
        mCompositorDelegate.onClick(mGuid, mViewportRect.left + x, mViewportRect.top + y);
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
            assert mBitmapMatrix.get(mRequestScaleFactor) != null;
            assert mBitmapMatrix.get(mRequestScaleFactor)[mRequestRow][mRequestCol] == null;
            assert mPendingBitmapRequests.get(mRequestScaleFactor)[mRequestRow][mRequestCol];

            mPendingBitmapRequests.get(mScaleFactor)[mRequestRow][mRequestCol] = false;
            if (mRequiredBitmaps.get(mRequestScaleFactor)[mRequestRow][mRequestCol]) {
                mBitmapMatrix.get(mScaleFactor)[mRequestRow][mRequestCol] = result;
                if (PlayerFrameMediator.this.mScaleFactor == mRequestScaleFactor) {
                    mModel.set(
                            PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix.get(mScaleFactor));
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
