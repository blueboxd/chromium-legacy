// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the {@link PlayerFrameBitmapPainter} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerFrameBitmapPainterTest {
    /**
     * Mocks {@link Canvas} and holds all calls to
     * {@link Canvas#drawBitmap(Bitmap, Rect, Rect, Paint)}.
     */
    private class MockCanvas extends Canvas {
        private List<DrawnBitmap> mDrawnBitmaps = new ArrayList<>();

        private class DrawnBitmap {
            private final Bitmap mBitmap;
            private final Rect mSrc;
            private final Rect mDst;

            private DrawnBitmap(Bitmap bitmap, Rect src, Rect dst) {
                mBitmap = bitmap;
                mSrc = new Rect(src);
                mDst = new Rect(dst);
            }

            @Override
            public boolean equals(Object o) {
                if (o == null) return false;

                if (this == o) return true;

                if (getClass() != o.getClass()) return false;

                DrawnBitmap od = (DrawnBitmap) o;
                return mBitmap.equals(od.mBitmap) && mSrc.equals(od.mSrc) && mDst.equals(od.mDst);
            }
        }

        @Override
        public void drawBitmap(@NonNull Bitmap bitmap, @Nullable Rect src, @NonNull Rect dst,
                @Nullable Paint paint) {
            mDrawnBitmaps.add(new DrawnBitmap(bitmap, src, dst));
        }

        /**
         * Asserts if a portion of a given bitmap has been drawn on this canvas.
         */
        private void assertDrawBitmap(
                @NonNull Bitmap bitmap, @Nullable Rect src, @NonNull Rect dst) {
            Assert.assertTrue(bitmap + " has not been drawn from " + src + " to " + dst,
                    mDrawnBitmaps.contains(new DrawnBitmap(bitmap, src, dst)));
        }

        /**
         * Asserts the number of bitmap draw operations on this canvas.
         */
        private void assertNumberOfBitmapDraws(int expected) {
            Assert.assertEquals(expected, mDrawnBitmaps.size());
        }
    }

    private Bitmap[][] generateMockBitmapMatrix(int rows, int cols) {
        Bitmap[][] matrix = new Bitmap[rows][cols];
        for (int row = 0; row < matrix.length; ++row) {
            for (int col = 0; col < matrix[row].length; ++col) {
                matrix[row][col] = Mockito.mock(Bitmap.class);
            }
        }
        return matrix;
    }

    /**
     * Verifies no draw operations are performed on the canvas if the view port is invalid.
     */
    @Test
    public void testDrawFaultyViewPort() {
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(Mockito.mock(Runnable.class));
        painter.updateBitmapMatrix(generateMockBitmapMatrix(2, 3));
        painter.updateViewPort(0, 5, 10, -10);

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(0);

        // Update the view port so it is covered by 2 bitmap tiles.
        painter.updateViewPort(0, 5, 10, 15);
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(2);
    }

    /**
     * Verifies no draw operations are performed on the canvas if the bitmap matrix is invalid.
     */
    @Test
    public void testDrawFaultyBitmapMatrix() {
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(Mockito.mock(Runnable.class));
        painter.updateBitmapMatrix(new Bitmap[0][0]);
        // This view port is covered by 2 bitmap tiles, so there should be 2 draw operations on
        // the canvas.
        painter.updateViewPort(0, 5, 10, 15);

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(0);

        painter.updateBitmapMatrix(generateMockBitmapMatrix(2, 1));
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(2);
    }

    /**
     * Verified {@link PlayerFrameBitmapPainter#onDraw} draws the right bitmap tiles, at the correct
     * coordinates, for the given view port.
     */
    @Test
    public void testDraw() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        PlayerFrameBitmapPainter painter = new PlayerFrameBitmapPainter(invalidator);

        // Prepare the bitmap matrix.
        Bitmap[][] bitmaps = new Bitmap[2][2];
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        bitmaps[0][0] = bitmap00;
        bitmaps[1][0] = bitmap10;
        bitmaps[0][1] = bitmap01;
        bitmaps[1][1] = bitmap11;

        painter.updateBitmapMatrix(bitmaps);
        painter.updateViewPort(5, 10, 15, 25);

        // Make sure the invalidator was called after updating the bitmap matrix and the view port.
        Mockito.verify(invalidator, Mockito.times(2)).run();

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        // Verify that the correct portions of each bitmap tiles is painted in the correct
        // positions of in the canvas.
        canvas.assertNumberOfBitmapDraws(4);
        canvas.assertDrawBitmap(bitmap00, new Rect(5, 10, 10, 15), new Rect(0, 0, 5, 5));
        canvas.assertDrawBitmap(bitmap10, new Rect(5, 0, 10, 10), new Rect(0, 5, 5, 15));
        canvas.assertDrawBitmap(bitmap01, new Rect(0, 10, 5, 15), new Rect(5, 0, 10, 5));
        canvas.assertDrawBitmap(bitmap11, new Rect(0, 0, 5, 10), new Rect(5, 5, 10, 15));
    }
}
