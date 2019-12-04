// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Pair;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
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
public class PlayerFrameMediatorTest {
    private static final long FRAME_GUID = 123321L;
    private static final int CONTENT_WIDTH = 560;
    private static final int CONTENT_HEIGHT = 1150;

    private PropertyModel mModel;
    private TestPlayerCompositorDelegate mCompositorDelegate;
    private PlayerFrameMediator mMediator;

    /**
     * Used for keeping track of all bitmap requests that {@link PlayerFrameMediator} makes.
     */
    private class RequestedBitmap {
        long mFrameGuid;
        Rect mClipRect;
        float mScaleFactor;
        Callback<Bitmap> mBitmapCallback;
        Runnable mErrorCallback;

        public RequestedBitmap(long frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            this.mFrameGuid = frameGuid;
            this.mClipRect = clipRect;
            this.mScaleFactor = scaleFactor;
            this.mBitmapCallback = bitmapCallback;
            this.mErrorCallback = errorCallback;
        }

        public RequestedBitmap(long frameGuid, Rect clipRect, float scaleFactor) {
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
            return rb.mClipRect.equals(mClipRect) && rb.mFrameGuid == mFrameGuid
                    && rb.mScaleFactor == mScaleFactor;
        }

        @Override
        public String toString() {
            return mFrameGuid + ", " + mClipRect + ", " + mScaleFactor;
        }
    }

    /**
     * Mocks {@link PlayerCompositorDelegate}. Stores all bitmap requests as
     * {@link RequestedBitmap}s.
     */
    private class TestPlayerCompositorDelegate implements PlayerCompositorDelegate {
        List<RequestedBitmap> mRequestedBitmap = new ArrayList<>();

        @Override
        public void requestBitmap(long frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            mRequestedBitmap.add(new RequestedBitmap(
                    frameGuid, new Rect(clipRect), scaleFactor, bitmapCallback, errorCallback));
        }

        @Override
        public void onClick(long frameGuid, Point point) {}
    }

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        mCompositorDelegate = new TestPlayerCompositorDelegate();
        mMediator = new PlayerFrameMediator(
                mModel, mCompositorDelegate, FRAME_GUID, CONTENT_WIDTH, CONTENT_HEIGHT);
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
        // columns.
        Bitmap[][] bitmapMatrix = mModel.get(PlayerFrameProperties.BITMAP_MATRIX);
        Assert.assertTrue(Arrays.deepEquals(bitmapMatrix, new Bitmap[6][4]));
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
        mMediator.setLayoutDimensions(100, 200);

        // Requests for bitmaps in all tiles that are visible in the view port should've been made.
        // Since the current view port fully matches the top left bitmap tile, that should be the
        // only requested bitmap.
        List<RequestedBitmap> expectedRequestedBitmaps = new ArrayList<>();
        expectedRequestedBitmaps.add(new RequestedBitmap(FRAME_GUID, new Rect(0, 0, 100, 200), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        mMediator.scrollBy(10, 20);
        // The view port was moved with the #updateViewport call. It should've been updated in the
        // model.
        Rect expectedViewPort = new Rect(10, 20, 110, 220);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // The current viewport covers portions of 4 adjacent bitmap tiles. Make sure requests for
        // compositing those bitmap tiles are made.
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(0, 200, 100, 400), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(100, 0, 200, 200), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(100, 200, 200, 400), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port slightly. It is still covered by the same 4 tiles. Since there were
        // already bitmap requests out for those tiles, we shouldn't have made new requests.
        mMediator.scrollBy(10, 20);
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port to the bottom right so it covers portions of the 4 bottom right bitmap
        // tiles. 4 new bitmap requests should be made.
        mMediator.scrollBy(430, 900);
        expectedViewPort.set(450, 940, 550, 1140);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(400, 800, 500, 1000), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(400, 1000, 500, 1200), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(500, 800, 600, 1000), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(500, 1000, 600, 1200), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
    }

    /**
     * Mocks responses on bitmap requests from {@link PlayerFrameMediator} and tests those responses
     * are correctly handled.
     */
    @Test
    public void testBitmapRequestResponse() {
        // Sets the bitmap tile size to 100x200 and triggers bitmap request for the upper left tile.
        mMediator.setLayoutDimensions(150, 200);

        // Create mock bitmap for response.
        Bitmap bitmap1 = Mockito.mock(Bitmap.class);
        Bitmap[][] expectedBitmapMatrix = new Bitmap[6][4];
        expectedBitmapMatrix[0][0] = bitmap1;

        // Call the request callback with the mock bitmap and assert it's added to the model.
        mCompositorDelegate.mRequestedBitmap.get(0).mBitmapCallback.onResult(bitmap1);
        Assert.assertTrue(Arrays.deepEquals(
                expectedBitmapMatrix, mModel.get(PlayerFrameProperties.BITMAP_MATRIX)));
        Assert.assertEquals(new ArrayList<Pair<View, Rect>>(),
                mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));

        // Move the viewport to an area that is covered by 3 additional tiles. Triggers bitmap
        // requests for an additional 3 tiles.
        mMediator.scrollBy(10, 10);
        // Assert that there are the only 4 total bitmap requests, i.e. we didn't request for the
        // tile at [0][0] again.
        List<RequestedBitmap> expectedRequestedBitmaps = new ArrayList<>();
        expectedRequestedBitmaps.add(new RequestedBitmap(FRAME_GUID, new Rect(0, 0, 150, 200), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(0, 200, 150, 400), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(150, 0, 300, 200), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(150, 200, 300, 400), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Mock a compositing failure for the second request.
        mCompositorDelegate.mRequestedBitmap.get(1).mErrorCallback.run();
        // Move the view port while staying within the 4 bitmap tiles in order to trigger the
        // request logic again. Make sure only a new request is added, for the tile with a
        // compositing failure.
        mMediator.scrollBy(10, 10);
        expectedRequestedBitmaps.add(
                new RequestedBitmap(FRAME_GUID, new Rect(0, 200, 150, 400), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
    }

    /**
     * View port should be updated on scroll events, but it shouldn't go out of content bounds.
     */
    @Test
    public void testViewPortOnScrollBy() {
        // Initial view port setup.
        mMediator.setLayoutDimensions(100, 200);
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
        mMediator.setLayoutDimensions(100, 200);
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
     * TODO(crbug.com/1020702): Implement after zooming support is added.
     */
    @Test
    public void testViewPortOnScaleBy() {}
}
