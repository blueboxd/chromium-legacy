// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Parcel;
import android.util.Pair;
import android.view.View;
import android.widget.Scroller;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for the {@link PlayerFrameMediator} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {PaintPreviewCustomFlingingShadowScroller.class})
public class PlayerFrameMediatorTest {
    private static final int CONTENT_WIDTH = 560;
    private static final int CONTENT_HEIGHT = 1150;

    private UnguessableToken mFrameGuid;
    private PropertyModel mModel;
    private TestPlayerCompositorDelegate mCompositorDelegate;
    private Scroller mScroller;
    private PlayerFrameMediator mMediator;

    /**
     * Generate an UnguessableToken with a static value.
     */
    private UnguessableToken frameGuid() {
        // Use a parcel for testing to avoid calling the normal native constructor.
        Parcel parcel = Parcel.obtain();
        parcel.writeLong(123321L);
        parcel.writeLong(987654L);
        parcel.setDataPosition(0);
        return UnguessableToken.CREATOR.createFromParcel(parcel);
    }

    /**
     * Used for keeping track of all bitmap requests that {@link PlayerFrameMediator} makes.
     */
    private class RequestedBitmap {
        UnguessableToken mFrameGuid;
        Rect mClipRect;
        float mScaleFactor;
        Callback<Bitmap> mBitmapCallback;
        Runnable mErrorCallback;

        public RequestedBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            this.mFrameGuid = frameGuid;
            this.mClipRect = clipRect;
            this.mScaleFactor = scaleFactor;
            this.mBitmapCallback = bitmapCallback;
            this.mErrorCallback = errorCallback;
        }

        public RequestedBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor) {
            this.mFrameGuid = frameGuid;
            this.mClipRect = clipRect;
            this.mScaleFactor = scaleFactor;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) return false;

            if (o == this) return true;

            if (o.getClass() != this.getClass()) return false;

            RequestedBitmap rb = (RequestedBitmap) o;
            return rb.mClipRect.equals(mClipRect) && rb.mFrameGuid.equals(mFrameGuid)
                    && rb.mScaleFactor == mScaleFactor;
        }

        @NonNull
        @Override
        public String toString() {
            return mFrameGuid + ", " + mClipRect + ", " + mScaleFactor;
        }
    }

    /**
     * Used for keeping track of all click events that {@link PlayerFrameMediator} sends to
     * {@link PlayerCompositorDelegate}.
     */
    private class ClickedPoint {
        UnguessableToken mFrameGuid;
        int mX;
        int mY;

        public ClickedPoint(UnguessableToken frameGuid, int x, int y) {
            mFrameGuid = frameGuid;
            this.mX = x;
            this.mY = y;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) return false;

            if (o == this) return true;

            if (o.getClass() != this.getClass()) return false;

            ClickedPoint cp = (ClickedPoint) o;
            return cp.mFrameGuid.equals(mFrameGuid) && cp.mX == mX && cp.mY == mY;
        }

        @NonNull
        @Override
        public String toString() {
            return "Click event for frame " + mFrameGuid.toString() + " on (" + mX + ", " + mY
                    + ")";
        }
    }

    /**
     * Mocks {@link PlayerCompositorDelegate}. Stores all bitmap requests as
     * {@link RequestedBitmap}s.
     */
    private class TestPlayerCompositorDelegate implements PlayerCompositorDelegate {
        List<RequestedBitmap> mRequestedBitmap = new ArrayList<>();
        List<ClickedPoint> mClickedPoints = new ArrayList<>();

        @Override
        public void requestBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            mRequestedBitmap.add(new RequestedBitmap(
                    frameGuid, new Rect(clipRect), scaleFactor, bitmapCallback, errorCallback));
        }

        @Override
        public void onClick(UnguessableToken frameGuid, int x, int y) {
            mClickedPoints.add(new ClickedPoint(frameGuid, x, y));
        }
    }

    @Before
    public void setUp() {
        mFrameGuid = frameGuid();
        mModel = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        mCompositorDelegate = new TestPlayerCompositorDelegate();
        mScroller = new Scroller(ContextUtils.getApplicationContext());
        mMediator = new PlayerFrameMediator(
                mModel, mCompositorDelegate, mScroller, mFrameGuid, CONTENT_WIDTH, CONTENT_HEIGHT);
    }

    private static Rect getRectForTile(int tileWidth, int tileHeight, int row, int col) {
        int left = col * tileWidth;
        int top = row * tileHeight;
        return new Rect(left, top, left + tileWidth, top + tileHeight);
    }

    /**
     * Tests that {@link PlayerFrameMediator} is initialized correctly on the first call to
     * {@link PlayerFrameMediator#setLayoutDimensions}.
     */
    @Test
    public void testInitialLayoutDimensions() {
        // Initial view port setup.
        mMediator.setLayoutDimensions(150, 200);

        // View port should be as big as size set in the first setLayoutDimensions call, showing
        // the top left corner.
        Rect expectedViewPort = new Rect(0, 0, 150, 200);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // The bitmap matrix should be empty, but initialized with the correct number of rows and
        // columns. Because we set the initial scale factor to view port width over content width,
        // we should have only one column.
        Bitmap[][] bitmapMatrix = mModel.get(PlayerFrameProperties.BITMAP_MATRIX);
        Assert.assertTrue(Arrays.deepEquals(bitmapMatrix, new Bitmap[2][1]));
        Assert.assertEquals(new ArrayList<Pair<View, Rect>>(),
                mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
    }

    /**
     * Tests that {@link PlayerFrameMediator} requests for the right bitmap tiles as the view port
     * moves.
     */
    @Test
    public void testBitmapRequest() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);

        // Requests for bitmaps in all tiles that are visible in the view port as well as their
        // adjacent tiles should've been made.
        // The current view port fully matches the top left bitmap tile, so we expect requests for
        // the top left bitmap, one bitmap to its right, and one to its bottom.
        // Below is a schematic of the entire bitmap matrix. Those marked with number should have
        // been requested, in the order of numbers.
        // -------------------------
        // | 1 | 3 |   |   |   |   |
        // -------------------------
        // | 2 |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        List<RequestedBitmap> expectedRequestedBitmaps = new ArrayList<>();
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 0, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 1, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 0, 1), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        mMediator.scrollBy(10, 20);
        // The view port was moved with the #updateViewport call. It should've been updated in the
        // model.
        Rect expectedViewPort = new Rect(10, 20, 110, 220);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // The current viewport covers portions of the 4 top left bitmap tiles. We have requested
        // bitmaps for 3 of them before. Make sure requests for the 4th bitmap, as well adjacent
        // bitmaps are made.
        // Below is a schematic of the entire bitmap matrix. Those marked with number should have
        // been requested, in the order of numbers.
        // -------------------------
        // | x | x | 3 |   |   |   |
        // -------------------------
        // | x | 1 | 5 |   |   |   |
        // -------------------------
        // | 2 | 4 |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 1, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 2, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 0, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 2, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 1, 2), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port slightly. It is still covered by the same 4 tiles. Since there were
        // already bitmap requests out for those tiles and their adjacent tiles, we shouldn't have
        // made new requests.
        mMediator.scrollBy(10, 20);
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port to the bottom right so it covers portions of the 4 bottom right bitmap
        // tiles. 4 new bitmap requests should be made.
        // Below is a schematic of the entire bitmap matrix. Those marked with number should have
        // been requested, in the order of numbers.
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // |   |   |   |   | 5 | 8 |
        // -------------------------
        // |   |   |   | 6 | 1 | 3 |
        // -------------------------
        // |   |   |   | 7 | 2 | 4 |
        mMediator.scrollBy(430, 900);
        expectedViewPort.set(450, 940, 550, 1140);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 4, 4), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 5, 4), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 4, 5), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 5, 5), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 3, 4), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 4, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 5, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(100, 200, 3, 5), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
    }

    /**
     * Tests that the mediator keeps around the required bitmaps and removes the unrequired bitmaps
     * when the view port changes. Required bitmaps are those in the viewport and its adjacent
     * tiles.
     */
    @Test
    public void testRequiredBitmapMatrix() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);

        boolean[][] expectedRequiredBitmaps = new boolean[6][6];

        // The current view port fully matches the top left bitmap tile.
        // Below is a schematic of the entire bitmap matrix. Tiles marked with x are required for
        // the current view port.
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // | x |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[1][0] = true;
        Assert.assertTrue(
                Arrays.deepEquals(expectedRequiredBitmaps, mMediator.mRequiredBitmaps.get(1f)));

        mMediator.scrollBy(10, 15);
        // The current viewport covers portions of the 4 top left bitmap tiles.
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        Assert.assertTrue(
                Arrays.deepEquals(expectedRequiredBitmaps, mMediator.mRequiredBitmaps.get(1f)));

        mMediator.scrollBy(200, 400);
        // The current view port contains portions of the middle 4 tiles.
        // Tiles marked with x are required for the current view port.
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   | x | x |   |   |
        // -------------------------
        // |   | x | x | x | x |   |
        // -------------------------
        // |   | x | x | x | x |   |
        // -------------------------
        // |   |   | x | x |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][0] = false;
        expectedRequiredBitmaps[0][1] = false;
        expectedRequiredBitmaps[0][2] = false;
        expectedRequiredBitmaps[1][0] = false;
        expectedRequiredBitmaps[1][1] = false;
        expectedRequiredBitmaps[2][0] = false;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[2][4] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        expectedRequiredBitmaps[3][3] = true;
        expectedRequiredBitmaps[3][4] = true;
        expectedRequiredBitmaps[4][2] = true;
        expectedRequiredBitmaps[4][3] = true;
        Assert.assertTrue(
                Arrays.deepEquals(expectedRequiredBitmaps, mMediator.mRequiredBitmaps.get(1f)));

        mMediator.scrollBy(200, 400);
        // The current view port contains portions of the 4 bottom right tiles.
        // Tiles marked with x are required for the current view port.
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   | x | x |
        // -------------------------
        // |   |   |   | x | x | x |
        // -------------------------
        // |   |   |   | x | x | x |
        expectedRequiredBitmaps[1][2] = false;
        expectedRequiredBitmaps[1][3] = false;
        expectedRequiredBitmaps[2][1] = false;
        expectedRequiredBitmaps[2][2] = false;
        expectedRequiredBitmaps[2][3] = false;
        expectedRequiredBitmaps[2][4] = false;
        expectedRequiredBitmaps[3][1] = false;
        expectedRequiredBitmaps[3][2] = false;
        expectedRequiredBitmaps[3][3] = false;
        expectedRequiredBitmaps[4][2] = false;

        expectedRequiredBitmaps[3][5] = true;
        expectedRequiredBitmaps[4][4] = true;
        expectedRequiredBitmaps[4][5] = true;
        expectedRequiredBitmaps[5][3] = true;
        expectedRequiredBitmaps[5][4] = true;
        expectedRequiredBitmaps[5][5] = true;
        Assert.assertTrue(
                Arrays.deepEquals(expectedRequiredBitmaps, mMediator.mRequiredBitmaps.get(1f)));
    }

    /**
     * Mocks responses on bitmap requests from {@link PlayerFrameMediator} and tests those responses
     * are correctly handled.
     */
    @Test
    public void testBitmapRequestResponse() {
        // Sets the bitmap tile size to 150x200 and triggers bitmap request for the upper left tile
        // and its adjacent tiles.
        mMediator.updateViewportSize(150, 200, 1f);

        // Create mock bitmaps for response.
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap20 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        Bitmap bitmap21 = Mockito.mock(Bitmap.class);
        Bitmap bitmap02 = Mockito.mock(Bitmap.class);
        Bitmap bitmap12 = Mockito.mock(Bitmap.class);

        Bitmap[][] expectedBitmapMatrix = new Bitmap[6][4];
        expectedBitmapMatrix[0][0] = bitmap00;
        expectedBitmapMatrix[1][0] = bitmap10;
        expectedBitmapMatrix[0][1] = bitmap01;

        // Call the request callback with mock bitmaps and assert they're added to the model.
        mCompositorDelegate.mRequestedBitmap.get(0).mBitmapCallback.onResult(bitmap00);
        mCompositorDelegate.mRequestedBitmap.get(1).mBitmapCallback.onResult(bitmap10);
        mCompositorDelegate.mRequestedBitmap.get(2).mBitmapCallback.onResult(bitmap01);
        Assert.assertTrue(Arrays.deepEquals(
                expectedBitmapMatrix, mModel.get(PlayerFrameProperties.BITMAP_MATRIX)));

        // Move the viewport to an area that is covered by 4 top left tiles.
        mMediator.scrollBy(10, 10);

        // Scroll should've triggered bitmap requests for an the 4th new tile as well as adjacent
        // tiles. See comments on {@link #testBitmapRequest} for details on which tiles will be
        // requested.
        // Call the request callback with mock bitmaps and assert they're added to the model.
        expectedBitmapMatrix[1][1] = bitmap11;
        expectedBitmapMatrix[0][2] = bitmap02;
        expectedBitmapMatrix[2][1] = bitmap21;
        expectedBitmapMatrix[1][2] = bitmap12;
        mCompositorDelegate.mRequestedBitmap.get(3).mBitmapCallback.onResult(bitmap11);
        // Mock a compositing failure for this tile. No bitmaps should be added.
        mCompositorDelegate.mRequestedBitmap.get(4).mErrorCallback.run();
        mCompositorDelegate.mRequestedBitmap.get(5).mBitmapCallback.onResult(bitmap02);
        mCompositorDelegate.mRequestedBitmap.get(6).mBitmapCallback.onResult(bitmap21);
        mCompositorDelegate.mRequestedBitmap.get(7).mBitmapCallback.onResult(bitmap12);
        Assert.assertTrue(Arrays.deepEquals(
                expectedBitmapMatrix, mModel.get(PlayerFrameProperties.BITMAP_MATRIX)));

        // Assert 8 bitmap requests have been made in total.
        Assert.assertEquals(8, mCompositorDelegate.mRequestedBitmap.size());

        // Move the view port while staying within the 4 bitmap tiles in order to trigger the
        // request logic again. Make sure only one new request is added, for the tile with a
        // compositing failure.
        mMediator.scrollBy(10, 10);
        Assert.assertEquals(9, mCompositorDelegate.mRequestedBitmap.size());
        Assert.assertEquals(new RequestedBitmap(mFrameGuid, getRectForTile(150, 200, 2, 0), 1f),
                mCompositorDelegate.mRequestedBitmap.get(
                        mCompositorDelegate.mRequestedBitmap.size() - 1));
    }

    /**
     * View port should be updated on scroll events, but it shouldn't go out of content bounds.
     */
    @Test
    public void testViewPortOnScrollBy() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        Rect expectedViewPort = new Rect(0, 0, 100, 200);

        // Scroll right and down by a within bounds amount. Both scroll directions should be
        // effective.
        Assert.assertTrue(mMediator.scrollBy(250f, 80f));
        expectedViewPort.offset(250, 80);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll by an out of bounds horizontal value. Should be scrolled to the rightmost point.
        Assert.assertTrue(mMediator.scrollBy(1000f, 50f));
        expectedViewPort.offset(210, 50);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll by an out of bounds horizontal and vertical value.
        // Should be scrolled to the bottom right point.
        Assert.assertTrue(mMediator.scrollBy(600f, 5000f));
        expectedViewPort.offset(0, 820);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll right and down. Should be impossible since we're already at the bottom right
        // point.
        Assert.assertFalse(mMediator.scrollBy(10f, 15f));
        expectedViewPort.offset(0, 0);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll right and up. Horizontal scroll should be ignored and should be scrolled to the
        // top.
        Assert.assertTrue(mMediator.scrollBy(100f, -2000f));
        expectedViewPort.offset(0, -950);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll right and up. Both scroll directions should be ignored.
        Assert.assertFalse(mMediator.scrollBy(100f, -2000f));
        expectedViewPort.offset(0, 0);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll left and up. Vertical scroll should be ignored and should be scrolled to the
        // left.
        Assert.assertTrue(mMediator.scrollBy(-1000f, -2000f));
        expectedViewPort.offset(-460, 0);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll left and up. Both scroll directions should be ignored.
        Assert.assertFalse(mMediator.scrollBy(-1000f, -2000f));
        expectedViewPort.offset(0, 0);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll left and down. Horizontal scroll should be ignored and should be scrolled to the
        // bottom.
        Assert.assertTrue(mMediator.scrollBy(-1000f, 2000f));
        expectedViewPort.offset(0, 950);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll left and down. Both scroll directions should be ignored.
        Assert.assertFalse(mMediator.scrollBy(-1000f, 2000f));
        expectedViewPort.offset(0, 0);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // Scroll right and up. Both scroll values should be reflected.
        Assert.assertTrue(mMediator.scrollBy(200, -100));
        expectedViewPort.offset(200, -100);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));
    }

    /**
     * Tests sub-frames' visibility when view port changes. sub-frames that are out of the view
     * port's bounds should not be added to the model.
     */
    @Test
    public void testSubFramesPosition() {
        Pair<View, Rect> subFrame1 =
                new Pair<>(Mockito.mock(View.class), new Rect(10, 20, 60, 120));
        Pair<View, Rect> subFrame2 =
                new Pair<>(Mockito.mock(View.class), new Rect(30, 130, 70, 160));
        Pair<View, Rect> subFrame3 =
                new Pair<>(Mockito.mock(View.class), new Rect(120, 35, 150, 65));

        mMediator.addSubFrame(subFrame1.first, subFrame1.second);
        mMediator.addSubFrame(subFrame2.first, subFrame2.second);
        mMediator.addSubFrame(subFrame3.first, subFrame3.second);

        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        List<Pair<View, Rect>> expectedVisibleViews = new ArrayList<>();
        expectedVisibleViews.add(subFrame1);
        expectedVisibleViews.add(subFrame2);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));

        mMediator.scrollBy(100, 0);
        expectedVisibleViews.clear();
        expectedVisibleViews.add(subFrame3);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));

        mMediator.scrollBy(-50, 0);
        expectedVisibleViews.clear();
        expectedVisibleViews.add(subFrame1);
        expectedVisibleViews.add(subFrame2);
        expectedVisibleViews.add(subFrame3);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));

        mMediator.scrollBy(0, 200);
        expectedVisibleViews.clear();
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
    }

    /**
     * View port should be updated on fling events, but it shouldn't go out of content bounds.
     */
    @Test
    public void testViewPortOnFling() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        Rect expectedViewPort = new Rect(0, 0, 100, 200);

        mMediator.onFling(100, 0);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        mMediator.onFling(-100, 0);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        mMediator.onFling(0, 200);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        mMediator.onFling(0, -200);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        mMediator.onFling(100, 200);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        mMediator.onFling(-100, -200);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));
    }

    /**
     * Tests that {@link PlayerFrameMediator} correctly relays the click events to
     * {@link PlayerCompositorDelegate} and accounts for scroll offsets.
     */
    @Test
    public void testOnClick() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        List<ClickedPoint> expectedClickedPoints = new ArrayList<>();

        // No scrolling has happened yet.
        mMediator.onClick(15, 26);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 15, 26));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);

        // Scroll, and then click. The call to {@link PlayerFrameMediator} must account for the
        // scroll offset.
        mMediator.scrollBy(90, 100);
        mMediator.onClick(70, 50);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 160, 150));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);

        mMediator.scrollBy(-40, -60);
        mMediator.onClick(30, 80);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 80, 120));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);
    }

    /**
     * TODO(crbug.com/1020702): Implement after zooming support is added.
     */
    @Test
    public void testViewPortOnScaleBy() {}
}
