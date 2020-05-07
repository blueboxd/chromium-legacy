// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;

import javax.annotation.Nonnull;

/**
 * Given a viewport {@link Rect} and a matrix of {@link Bitmap} tiles, this class draws the bitmaps
 * on a {@link Canvas}.
 */
class PlayerFrameBitmapPainter {
    private int mTileWidth;
    private int mTileHeight;
    private Bitmap[][] mBitmapMatrix;
    private Rect mViewPort = new Rect();
    private Rect mDrawBitmapSrc = new Rect();
    private Rect mDrawBitmapDst = new Rect();
    private Runnable mInvalidateCallback;

    PlayerFrameBitmapPainter(@Nonnull Runnable invalidateCallback) {
        mInvalidateCallback = invalidateCallback;
    }

    void updateTileDimensions(int[] tileDimensions) {
        mTileWidth = tileDimensions[0];
        mTileHeight = tileDimensions[1];
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        mViewPort.set(left, top, right, bottom);
        mInvalidateCallback.run();
    }

    void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        mBitmapMatrix = bitmapMatrix;
        mInvalidateCallback.run();
    }

    /**
     * Draws bitmaps on a given {@link Canvas} for the current viewport.
     */
    void onDraw(Canvas canvas) {
        if (mBitmapMatrix == null) return;

        if (mViewPort.isEmpty()) return;

        if (mTileWidth <= 0 || mTileHeight <= 0) return;

        final int rowStart = mViewPort.top / mTileHeight;
        final int rowEnd = (int) Math.ceil((double) mViewPort.bottom / mTileHeight);
        final int colStart = mViewPort.left / mTileWidth;
        final int colEnd = (int) Math.ceil((double) mViewPort.right / mTileWidth);
        if (rowEnd > mBitmapMatrix.length || colEnd > mBitmapMatrix[rowEnd - 1].length) {
            return;
        }

        for (int row = rowStart; row < rowEnd; row++) {
            for (int col = colStart; col < colEnd; col++) {
                Bitmap tileBitmap = mBitmapMatrix[row][col];
                if (tileBitmap == null) {
                    continue;
                }

                // Calculate the portion of this tileBitmap that is visible in mViewPort.
                int bitmapLeft = Math.max(mViewPort.left - (col * mTileWidth), 0);
                int bitmapTop = Math.max(mViewPort.top - (row * mTileHeight), 0);
                int bitmapRight =
                        Math.min(mTileWidth, bitmapLeft + mViewPort.right - (col * mTileWidth));
                int bitmapBottom =
                        Math.min(mTileHeight, bitmapTop + mViewPort.bottom - (row * mTileHeight));
                mDrawBitmapSrc.set(bitmapLeft, bitmapTop, bitmapRight, bitmapBottom);

                // Calculate the portion of the canvas that tileBitmap is gonna be drawn on.
                int canvasLeft = Math.max((col * mTileWidth) - mViewPort.left, 0);
                int canvasTop = Math.max((row * mTileHeight) - mViewPort.top, 0);
                int canvasRight = canvasLeft + mDrawBitmapSrc.width();
                int canvasBottom = canvasTop + mDrawBitmapSrc.height();
                mDrawBitmapDst.set(canvasLeft, canvasTop, canvasRight, canvasBottom);

                canvas.drawBitmap(tileBitmap, mDrawBitmapSrc, mDrawBitmapDst, null);
            }
        }
    }
}
