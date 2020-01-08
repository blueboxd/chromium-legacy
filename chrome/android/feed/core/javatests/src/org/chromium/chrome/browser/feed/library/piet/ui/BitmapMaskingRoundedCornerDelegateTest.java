// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache.Corner;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache.RoundedCornerBitmaps;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link BitmapMaskingRoundedCornerDelegate}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BitmapMaskingRoundedCornerDelegateTest {
    @Mock
    private Canvas mMockCanvas;
    @Mock
    private RoundedCornerMaskCache mCache;

    private static final Supplier<Boolean> IS_RTL_SUPPLIER = Suppliers.of(false);
    private RoundedCornerWrapperView mRoundedCornerWrapperView;
    private Context mContext;
    private static final int RADIUS = 10;

    private Bitmap mTopLeft;
    private Bitmap mTopRight;
    private Bitmap mBottomLeft;
    private Bitmap mBottomRight;
    private RoundedCornerBitmaps mBitmaps;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mRoundedCornerWrapperView =
                new RoundedCornerWrapperView(mContext, RoundedCorners.getDefaultInstance(), mCache,
                        IS_RTL_SUPPLIER, 0, Borders.getDefaultInstance(), true, true);

        RoundedCornerMaskCache realCache = new RoundedCornerMaskCache();
        mBitmaps = realCache.getMasks(16);
        mTopLeft = realCache.getMasks(16).get(Corner.TOP_LEFT);
        mTopRight = realCache.getMasks(16).get(Corner.TOP_RIGHT);
        mBottomLeft = realCache.getMasks(16).get(Corner.BOTTOM_LEFT);
        mBottomRight = realCache.getMasks(16).get(Corner.BOTTOM_RIGHT);

        when(mCache.getMasks(anyInt())).thenReturn(mBitmaps);
        when(mCache.getMaskPaint()).thenReturn(new Paint());
    }

    @Test
    public void maskCorners_radiusZero() {
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        mCache, /* bitmask= */ 15, /* isRtL= */ false, mMockCanvas);

        mRoundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(/* radius= */ 0, /* isRtL= */ false, 100, 100);
        bitmapMaskingDelegate.draw(mRoundedCornerWrapperView, new Canvas());

        verify(mMockCanvas, never())
                .drawBitmap(any(Bitmap.class), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskAndDrawCorners_allCorners() {
        int all_corner_bitmask = 15;
        boolean isRtL = false;
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        mCache, all_corner_bitmask, isRtL, mMockCanvas);

        mRoundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(mRoundedCornerWrapperView, new Canvas());

        verify(mMockCanvas).drawBitmap(eq(mTopLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mTopRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mBottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mBottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskCorners_topStart_bottomEnd() {
        int topStart_bottomEnd_bitmask = 5;
        boolean isRtL = false;

        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        mCache, topStart_bottomEnd_bitmask, isRtL, mMockCanvas);

        mRoundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(mRoundedCornerWrapperView, new Canvas());

        verify(mMockCanvas).drawBitmap(eq(mTopLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas, never())
                .drawBitmap(eq(mTopRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mBottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas, never())
                .drawBitmap(eq(mBottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskCorners_topStart_bottomEnd_rtl() {
        int topStart_bottomEnd_bitmask = 5;
        boolean isRtL = true;
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        mCache, topStart_bottomEnd_bitmask, isRtL, mMockCanvas);

        mRoundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(mRoundedCornerWrapperView, new Canvas());

        verify(mMockCanvas, never())
                .drawBitmap(eq(mTopLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mTopRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas, never())
                .drawBitmap(eq(mBottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mMockCanvas).drawBitmap(eq(mBottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }
}
