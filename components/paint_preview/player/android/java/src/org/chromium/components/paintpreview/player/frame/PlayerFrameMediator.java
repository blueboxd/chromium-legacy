// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.Pair;
import android.view.View;

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
    /** The user-visible area for this frame. */
    private final Rect mViewportRect = new Rect();
    /** Rect used for requesting a new bitmap from Paint Preview compositor. */
    private final Rect mBitmapRequestRect = new Rect();
    /**
     * A scale factor cache of matrices of bitmaps that make up the content of this frame at a
     * given scale factor.
     */
    private final Map<Float, Bitmap[][]> mBitmapMatrix = new HashMap<>();
    /** Whether a request for a bitmap tile is pending, mapped by scale factor. */
    private final Map<Float, boolean[][]> mPendingBitmapRequests = new HashMap<>();
    /** The current scale factor. */
    private float mScaleFactor;

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            UnguessableToken frameGuid, int contentWidth, int contentHeight) {
        mModel = model;
        mCompositorDelegate = compositorDelegate;
        mGuid = frameGuid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
    }

    /**
     * Adds a new sub-frame to this frame.
     * @param subFrameView The {@link View} associated with the sub-frame.
     * @param clipRect The bounds of the sub-frame, relative to this frame.
     */
    void addSubFrame(View subFrameView, Rect clipRect) {
        mSubFrames.add(new Pair<>(subFrameView, clipRect));
    }

    @Override
    public void setLayoutDimensions(int width, int height) {
        // If the dimensions of mViewportRect has been set, we don't need to do anything.
        if (!mViewportRect.isEmpty() || width <= 0 || height <= 0) return;

        // Set mViewportRect's dimensions and start compositing.
        mViewportRect.set(0, 0, width, height);
        updateViewport(0, 0, 1f);
    }

    /**
     * Called when either the view port of the scale factor should be changed. Updates the view port
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param distanceX The horizontal distance that the view port should be moved by.
     * @param distanceY The vertical distance that the view port should be moved by.
     * @param scaleFactor The new scale factor.
     */
    private void updateViewport(int distanceX, int distanceY, float scaleFactor) {
        // TODO(crbug.com/1021111): Implement a caching mechanism that (i) fetches nearby tiles, and
        // (ii) destroys bitmaps that are unlikely to be used soon.

        // Initialize the bitmap matrix for this scale factor if we haven't already.
        Bitmap[][] bitmapMatrix = mBitmapMatrix.get(scaleFactor);
        boolean[][] pendingBitmapRequests = mPendingBitmapRequests.get(scaleFactor);
        if (bitmapMatrix == null) {
            // Each tile is as big as the view port. Here we determine the number of columns and
            // rows for the current scale factor.
            int rows = (int) Math.ceil((mContentHeight * scaleFactor) / mViewportRect.height());
            int cols = (int) Math.ceil((mContentWidth * scaleFactor) / mViewportRect.width());
            bitmapMatrix = new Bitmap[rows][cols];
            mBitmapMatrix.put(scaleFactor, bitmapMatrix);
            pendingBitmapRequests = new boolean[rows][cols];
            mPendingBitmapRequests.put(scaleFactor, pendingBitmapRequests);
        }

        // If the scale factor is changed, the view should get the correct bitmap matrix.
        if (scaleFactor != mScaleFactor) {
            mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix.get(scaleFactor));
            mScaleFactor = scaleFactor;
        }

        // Update mViewportRect and let the view know. PropertyModelChangeProcessor is smart about
        // this and will only update the view if mViewportRect is actually changed.
        mViewportRect.offset(distanceX, distanceY);
        mModel.set(PlayerFrameProperties.VIEWPORT, mViewportRect);

        // Request bitmaps for tiles inside the view port that don't already have a bitmap.
        final int tileWidth = mViewportRect.width();
        final int tileHeight = mViewportRect.height();
        final int colStart = mViewportRect.left / tileWidth;
        final int colEnd = (int) Math.ceil((double) mViewportRect.right / tileWidth);
        final int rowStart = mViewportRect.top / tileHeight;
        final int rowEnd = (int) Math.ceil((double) mViewportRect.bottom / tileHeight);
        for (int col = colStart; col < colEnd; col++) {
            for (int row = rowStart; row < rowEnd; row++) {
                if (bitmapMatrix[row][col] == null && !pendingBitmapRequests[row][col]) {
                    int tileLeft = col * tileWidth;
                    int tileTop = row * tileHeight;
                    mBitmapRequestRect.set(
                            tileLeft, tileTop, tileLeft + tileWidth, tileTop + tileHeight);
                    BitmapRequestHandler bitmapRequestHandler =
                            new BitmapRequestHandler(row, col, scaleFactor);
                    pendingBitmapRequests[row][col] = true;
                    mCompositorDelegate.requestBitmap(mGuid, mBitmapRequestRect, scaleFactor,
                            bitmapRequestHandler, bitmapRequestHandler);
                }
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

    /**
     * Called on scroll events from the user. Checks if scrolling is possible, and if so, calls
     * {@link #updateViewport}.
     * @param distanceX Horizontal scroll distance in pixels.
     * @param distanceY Vertical scroll distance in pixels.
     * @return Whether the scrolling was possible and view port was updated.
     */
    @Override
    public boolean scrollBy(float distanceX, float distanceY) {
        // TODO(crbug.com/1020702): These values should be scaled for scale factors other than 1.
        int validDistanceX = 0;
        int validDistanceY = 0;

        if (mViewportRect.left > 0 && distanceX < 0) {
            validDistanceX = (int) Math.max(distanceX, -1f * mViewportRect.left);
        } else if (mViewportRect.right < mContentWidth && distanceX > 0) {
            validDistanceX = (int) Math.min(distanceX, (float) mContentWidth - mViewportRect.right);
        }
        if (mViewportRect.top > 0 && distanceY < 0) {
            validDistanceY = (int) Math.max(distanceY, -1f * mViewportRect.top);
        } else if (mViewportRect.bottom < mContentHeight && distanceY > 0) {
            validDistanceY =
                    (int) Math.min(distanceY, (float) mContentHeight - mViewportRect.bottom);
        }

        if (validDistanceX == 0 && validDistanceY == 0) return false;

        updateViewport(validDistanceX, validDistanceY, mScaleFactor);
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

            mBitmapMatrix.get(mScaleFactor)[mRequestRow][mRequestCol] = result;
            mPendingBitmapRequests.get(mScaleFactor)[mRequestRow][mRequestCol] = false;
            if (PlayerFrameMediator.this.mScaleFactor == mRequestScaleFactor) {
                mModel.set(PlayerFrameProperties.BITMAP_MATRIX, mBitmapMatrix.get(mScaleFactor));
            }
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
