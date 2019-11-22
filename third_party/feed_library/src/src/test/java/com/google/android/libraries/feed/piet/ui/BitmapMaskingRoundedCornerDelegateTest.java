// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

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

import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache.Corner;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache.RoundedCornerBitmaps;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.Borders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for the {@link BitmapMaskingRoundedCornerDelegate}. */
@RunWith(RobolectricTestRunner.class)
public class BitmapMaskingRoundedCornerDelegateTest {
    @Mock
    private Canvas mockCanvas;
    @Mock
    private RoundedCornerMaskCache cache;

    private static final Supplier<Boolean> IS_RTL_SUPPLIER = Suppliers.of(false);
    private RoundedCornerWrapperView roundedCornerWrapperView;
    private Context context;
    private static final int RADIUS = 10;

    private Bitmap topLeft;
    private Bitmap topRight;
    private Bitmap bottomLeft;
    private Bitmap bottomRight;
    private RoundedCornerBitmaps bitmaps;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        roundedCornerWrapperView =
                new RoundedCornerWrapperView(context, RoundedCorners.getDefaultInstance(), cache,
                        IS_RTL_SUPPLIER, 0, Borders.getDefaultInstance(), true, true);

        RoundedCornerMaskCache realCache = new RoundedCornerMaskCache();
        bitmaps = realCache.getMasks(16);
        topLeft = realCache.getMasks(16).get(Corner.TOP_LEFT);
        topRight = realCache.getMasks(16).get(Corner.TOP_RIGHT);
        bottomLeft = realCache.getMasks(16).get(Corner.BOTTOM_LEFT);
        bottomRight = realCache.getMasks(16).get(Corner.BOTTOM_RIGHT);

        when(cache.getMasks(anyInt())).thenReturn(bitmaps);
        when(cache.getMaskPaint()).thenReturn(new Paint());
    }

    @Test
    public void maskCorners_radiusZero() {
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        cache, /* bitmask= */ 15, /* isRtL= */ false, mockCanvas);

        roundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(/* radius= */ 0, /* isRtL= */ false, 100, 100);
        bitmapMaskingDelegate.draw(roundedCornerWrapperView, new Canvas());

        verify(mockCanvas, never())
                .drawBitmap(any(Bitmap.class), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskAndDrawCorners_allCorners() {
        int all_corner_bitmask = 15;
        boolean isRtL = false;
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        cache, all_corner_bitmask, isRtL, mockCanvas);

        roundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(roundedCornerWrapperView, new Canvas());

        verify(mockCanvas).drawBitmap(eq(topLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(topRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(bottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(bottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskCorners_topStart_bottomEnd() {
        int topStart_bottomEnd_bitmask = 5;
        boolean isRtL = false;

        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        cache, topStart_bottomEnd_bitmask, isRtL, mockCanvas);

        roundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(roundedCornerWrapperView, new Canvas());

        verify(mockCanvas).drawBitmap(eq(topLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas, never())
                .drawBitmap(eq(topRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(bottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas, never())
                .drawBitmap(eq(bottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void maskCorners_topStart_bottomEnd_rtl() {
        int topStart_bottomEnd_bitmask = 5;
        boolean isRtL = true;
        BitmapMaskingRoundedCornerDelegate bitmapMaskingDelegate =
                new BitmapMaskingRoundedCornerDelegate(
                        cache, topStart_bottomEnd_bitmask, isRtL, mockCanvas);

        roundedCornerWrapperView.layout(0, 0, 100, 100);

        bitmapMaskingDelegate.onLayout(RADIUS, isRtL, 100, 100);
        bitmapMaskingDelegate.draw(roundedCornerWrapperView, new Canvas());

        verify(mockCanvas, never())
                .drawBitmap(eq(topLeft), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(topRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas, never())
                .drawBitmap(eq(bottomRight), anyFloat(), anyFloat(), any(Paint.class));
        verify(mockCanvas).drawBitmap(eq(bottomLeft), anyFloat(), anyFloat(), any(Paint.class));
    }
}
