// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View.MeasureSpec;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link AspectRatioScalingImageView}. */
@RunWith(RobolectricTestRunner.class)
public class AspectRatioScalingImageViewTest {
    private static final int DRAWABLE_WIDTH = 80;
    private static final int DRAWABLE_HEIGHT = 40;
    private static final int DRAWABLE_ASPECT_RATIO = 2;

    private static final int CONTAINER_WIDTH = 90;
    private static final int CONTAINER_HEIGHT = 30;

    private AspectRatioScalingImageView view;
    private Drawable drawable;

    @Before
    public void setUp() {
        view = new AspectRatioScalingImageView(Robolectric.buildActivity(Activity.class).get());
        drawable = new BitmapDrawable(
                Bitmap.createBitmap(DRAWABLE_WIDTH, DRAWABLE_HEIGHT, Bitmap.Config.RGB_565));
    }

    @Test
    public void testScaling_noDrawable_exactly() {
        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_noDrawable_unspecified() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(0);
        assertThat(view.getMeasuredWidth()).isEqualTo(0);
    }

    @Test
    public void testScaling_noDrawable_defaultAspectRatio() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);
        float aspectRatio = 2.0f;

        view.setDefaultAspectRatio(aspectRatio);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo((int) (CONTAINER_WIDTH / aspectRatio));
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_drawableBadDims_defaultAspectRatio() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);
        float aspectRatio = 2.0f;

        drawable = new ColorDrawable(Color.RED);

        view.setDefaultAspectRatio(aspectRatio);
        view.setImageDrawable(drawable);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo((int) (CONTAINER_WIDTH / aspectRatio));
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_bothExactly() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyWidth_unspecifiedHeight() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_WIDTH / DRAWABLE_ASPECT_RATIO);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyHeight_unspecifiedWidth() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_exactlyWidth_atMostHeight() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyHeight_atMostWidth() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.AT_MOST);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_atMostHeightAndWidth_widerContainer() {
        drawable = new BitmapDrawable(Bitmap.createBitmap(50, 100, Bitmap.Config.RGB_565));
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(200);
        assertThat(view.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testScaling_atMostHeightAndWidth_tallerContainer() {
        drawable = new BitmapDrawable(Bitmap.createBitmap(100, 50, Bitmap.Config.RGB_565));
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(100);
        assertThat(view.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testScaling_atMostWidth_unspecifiedHeight() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.AT_MOST);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_WIDTH / DRAWABLE_ASPECT_RATIO);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_atMostHeight_unspecifiedWidth() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(view.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_bothUnspecified() {
        view.setImageDrawable(drawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        view.measure(widthSpec, heightSpec);

        assertThat(view.getMeasuredHeight()).isEqualTo(0xFFFFFF);
        assertThat(view.getMeasuredWidth()).isEqualTo(0xFFFFFF);
    }
}
