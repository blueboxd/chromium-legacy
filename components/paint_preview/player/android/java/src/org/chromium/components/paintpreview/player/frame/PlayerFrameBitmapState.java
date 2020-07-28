// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;

import java.util.HashSet;
import java.util.Set;

/**
 * Manages the bitmaps shown in the PlayerFrameView at a given scale factor.
 */
public class PlayerFrameBitmapState {
    private final UnguessableToken mGuid;
    /** Dimension of tiles. */
    private final Size mTileSize;
    /** The scale factor of bitmaps. */
    private float mScaleFactor;
    /** Bitmaps that make up the contents. */
    private Bitmap[][] mBitmapMatrix;
    /** Whether a request for a bitmap tile is pending. */
    private boolean[][] mPendingBitmapRequests;
    /**
     * Whether we currently need a bitmap tile. This is used for deleting bitmaps that we don't
     * need and freeing up memory.
     */
    private boolean[][] mRequiredBitmaps;
    /** Delegate for accessing native to request bitmaps. */
    private final PlayerCompositorDelegate mCompositorDelegate;
    private final PlayerFrameBitmapStateController mStateController;
    private Set<Integer> mInitialMissingVisibleBitmaps = new HashSet<>();

    PlayerFrameBitmapState(UnguessableToken guid, int tileWidth, int tileHeight, float scaleFactor,
            Size contentSize, PlayerCompositorDelegate compositorDelegate,
            PlayerFrameBitmapStateController stateController) {
        mGuid = guid;
        mTileSize = new Size(tileWidth, tileHeight);
        mScaleFactor = scaleFactor;
        mCompositorDelegate = compositorDelegate;
        mStateController = stateController;

        // Each tile is as big as the initial view port. Here we determine the number of
        // columns and rows for the current scale factor.
        int rows = (int) Math.ceil((contentSize.getHeight() * scaleFactor) / tileHeight);
        int cols = (int) Math.ceil((contentSize.getWidth() * scaleFactor) / tileWidth);

        mBitmapMatrix = new Bitmap[rows][cols];
        mPendingBitmapRequests = new boolean[rows][cols];
        mRequiredBitmaps = new boolean[rows][cols];
    }

    @VisibleForTesting
    boolean[][] getRequiredBitmapsForTest() {
        return mRequiredBitmaps;
    }

    Bitmap[][] getMatrix() {
        return mBitmapMatrix;
    }

    Size getTileDimensions() {
        return mTileSize;
    }

    /**
     * Locks the state out of further updates.
     */
    void lock() {
        mRequiredBitmaps = null;
    }

    /**
     * Returns whether this state can be updated.
     */
    boolean isLocked() {
        return mRequiredBitmaps == null && mBitmapMatrix != null;
    }

    /**
     * Clears state so in-flight requests abort upon return.
     */
    void clear() {
        mBitmapMatrix = null;
        mRequiredBitmaps = null;
        mPendingBitmapRequests = null;
    }

    /**
     * Whether this bitmap state has loaded all the initial bitmaps.
     */
    boolean isReadyToShow() {
        return mInitialMissingVisibleBitmaps == null;
    }

    /**
     * Skips waiting for all visible bitmaps before showing.
     */
    void skipWaitingForVisibleBitmaps() {
        mInitialMissingVisibleBitmaps = null;
    }

    /**
     * Clears all the required bitmaps before they are re-set in {@link #requestBitmapForRect()}
     */
    void clearRequiredBitmaps() {
        if (mRequiredBitmaps == null) return;

        for (int row = 0; row < mRequiredBitmaps.length; row++) {
            for (int col = 0; col < mRequiredBitmaps[row].length; col++) {
                mRequiredBitmaps[row][col] = false;
            }
        }
    }

    /**
     * Requests bitmaps for tiles that overlap with the provided rect. Also requests bitmaps for
     * adjacent tiles.
     * @param viewportRect The rect of the viewport for which bitmaps are needed.
     */
    void requestBitmapForRect(Rect viewportRect) {
        if (mRequiredBitmaps == null || mBitmapMatrix == null) return;

        final int rowStart =
                Math.max(0, (int) Math.floor((double) viewportRect.top / mTileSize.getHeight()));
        final int rowEnd = Math.min(mRequiredBitmaps.length,
                (int) Math.ceil((double) viewportRect.bottom / mTileSize.getHeight()));

        final int colStart =
                Math.max(0, (int) Math.floor((double) viewportRect.left / mTileSize.getWidth()));
        final int colEnd = Math.min(mRequiredBitmaps[0].length,
                (int) Math.ceil((double) viewportRect.right / mTileSize.getWidth()));

        for (int col = colStart; col < colEnd; col++) {
            for (int row = rowStart; row < rowEnd; row++) {
                if (requestBitmapForTile(row, col) && mInitialMissingVisibleBitmaps != null) {
                    mInitialMissingVisibleBitmaps.add(row * mBitmapMatrix.length + col);
                }
            }
        }

        // Request bitmaps for adjacent tiles that are not currently in the view port. The reason
        // that we do this in a separate loop is to make sure bitmaps for tiles inside the view port
        // are fetched first.
        for (int col = colStart; col < colEnd; col++) {
            for (int row = rowStart; row < rowEnd; row++) {
                requestBitmapForAdjacentTiles(row, col);
            }
        }
    }

    private void requestBitmapForAdjacentTiles(int row, int col) {
        if (mBitmapMatrix == null) return;

        if (row > 0) {
            requestBitmapForTile(row - 1, col);
        }
        if (row < mBitmapMatrix.length - 1) {
            requestBitmapForTile(row + 1, col);
        }
        if (col > 0) {
            requestBitmapForTile(row, col - 1);
        }
        if (col < mBitmapMatrix[row].length - 1) {
            requestBitmapForTile(row, col + 1);
        }
    }

    private boolean requestBitmapForTile(int row, int col) {
        if (mRequiredBitmaps == null) return false;

        mRequiredBitmaps[row][col] = true;
        if (mBitmapMatrix == null || mPendingBitmapRequests == null
                || mBitmapMatrix[row][col] != null || mPendingBitmapRequests[row][col]) {
            return false;
        }

        final int y = row * mTileSize.getHeight();
        final int x = col * mTileSize.getWidth();

        BitmapRequestHandler bitmapRequestHandler =
                new BitmapRequestHandler(row, col, mScaleFactor);
        mPendingBitmapRequests[row][col] = true;
        mCompositorDelegate.requestBitmap(mGuid,
                new Rect(x, y, x + mTileSize.getWidth(), y + mTileSize.getHeight()), mScaleFactor,
                bitmapRequestHandler, bitmapRequestHandler::onError);
        return true;
    }

    /**
     * Remove previously fetched bitmaps that are no longer required according to
     * {@link #mRequiredBitmaps}.
     */
    private void deleteUnrequiredBitmaps() {
        if (mBitmapMatrix == null || mRequiredBitmaps == null) return;

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
     * Marks the bitmap at row and col as being loaded. If all bitmaps that were initially requested
     * for loading are present then this swaps the currently loading bitmap state to be the visible
     * bitmap state.
     * @param row The row of the bitmap that was loaded.
     * @param col The column of the bitmap that was loaded.
     */
    private void markBitmapReceived(int row, int col) {
        if (mBitmapMatrix == null) return;

        if (mInitialMissingVisibleBitmaps != null) {
            mInitialMissingVisibleBitmaps.remove(row * mBitmapMatrix.length + col);
            if (!mInitialMissingVisibleBitmaps.isEmpty()) return;

            mInitialMissingVisibleBitmaps = null;
        }

        mStateController.stateUpdated(this);
    }

    /**
     * Used as the callback for bitmap requests from the Paint Preview compositor.
     */
    private class BitmapRequestHandler implements Callback<Bitmap> {
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
                onError();
                return;
            }
            if (mBitmapMatrix == null || mPendingBitmapRequests == null || mRequiredBitmaps == null
                    || !mPendingBitmapRequests[mRequestRow][mRequestCol]
                    || !mRequiredBitmaps[mRequestRow][mRequestCol]) {
                markBitmapReceived(mRequestRow, mRequestCol);
                result.recycle();
                deleteUnrequiredBitmaps();
                return;
            }

            mPendingBitmapRequests[mRequestRow][mRequestCol] = false;
            mBitmapMatrix[mRequestRow][mRequestCol] = result;
            markBitmapReceived(mRequestRow, mRequestCol);
            deleteUnrequiredBitmaps();
        }

        /**
         * Called when there was an error compositing the bitmap.
         */
        public void onError() {
            markBitmapReceived(mRequestRow, mRequestCol);

            if (mPendingBitmapRequests == null) return;

            // TODO(crbug.com/1021590): Handle errors.
            assert mBitmapMatrix != null;
            assert mBitmapMatrix[mRequestRow][mRequestCol] == null;
            assert mPendingBitmapRequests[mRequestRow][mRequestCol];

            mPendingBitmapRequests[mRequestRow][mRequestCol] = false;
        }
    }

    @VisibleForTesting
    public boolean checkRequiredBitmapsLoadedForTest() {
        if (mBitmapMatrix == null || mRequiredBitmaps == null) return false;

        for (int row = 0; row < mBitmapMatrix.length; row++) {
            for (int col = 0; col < mBitmapMatrix[0].length; col++) {
                if (mRequiredBitmaps[row][col] && mBitmapMatrix[row][col] == null) {
                    return false;
                }
            }
        }
        return true;
    }
}
