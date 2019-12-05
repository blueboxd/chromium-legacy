// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

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
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link AspectRatioScalingImageView}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AspectRatioScalingImageViewTest {
    private static final int DRAWABLE_WIDTH = 80;
    private static final int DRAWABLE_HEIGHT = 40;
    private static final int DRAWABLE_ASPECT_RATIO = 2;

    private static final int CONTAINER_WIDTH = 90;
    private static final int CONTAINER_HEIGHT = 30;

    private AspectRatioScalingImageView mView;
    private Drawable mDrawable;

    @Before
    public void setUp() {
        mView = new AspectRatioScalingImageView(Robolectric.buildActivity(Activity.class).get());
        mDrawable = new BitmapDrawable(
                Bitmap.createBitmap(DRAWABLE_WIDTH, DRAWABLE_HEIGHT, Bitmap.Config.RGB_565));
    }

    @Test
    public void testScaling_noDrawable_exactly() {
        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_noDrawable_unspecified() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(0);
        assertThat(mView.getMeasuredWidth()).isEqualTo(0);
    }

    @Test
    public void testScaling_noDrawable_defaultAspectRatio() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);
        float aspectRatio = 2.0f;

        mView.setDefaultAspectRatio(aspectRatio);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo((int) (CONTAINER_WIDTH / aspectRatio));
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_drawableBadDims_defaultAspectRatio() {
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);
        float aspectRatio = 2.0f;

        mDrawable = new ColorDrawable(Color.RED);

        mView.setDefaultAspectRatio(aspectRatio);
        mView.setImageDrawable(mDrawable);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo((int) (CONTAINER_WIDTH / aspectRatio));
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_bothExactly() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyWidth_unspecifiedHeight() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_WIDTH / DRAWABLE_ASPECT_RATIO);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyHeight_unspecifiedWidth() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_exactlyWidth_atMostHeight() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.EXACTLY);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_exactlyHeight_atMostWidth() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.EXACTLY);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.AT_MOST);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_atMostHeightAndWidth_widerContainer() {
        mDrawable = new BitmapDrawable(Bitmap.createBitmap(50, 100, Bitmap.Config.RGB_565));
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(200);
        assertThat(mView.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testScaling_atMostHeightAndWidth_tallerContainer() {
        mDrawable = new BitmapDrawable(Bitmap.createBitmap(100, 50, Bitmap.Config.RGB_565));
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(100);
        assertThat(mView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testScaling_atMostWidth_unspecifiedHeight() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, MeasureSpec.AT_MOST);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_WIDTH / DRAWABLE_ASPECT_RATIO);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_WIDTH);
    }

    @Test
    public void testScaling_atMostHeight_unspecifiedWidth() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(CONTAINER_HEIGHT, MeasureSpec.AT_MOST);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(CONTAINER_HEIGHT);
        assertThat(mView.getMeasuredWidth()).isEqualTo(CONTAINER_HEIGHT * DRAWABLE_ASPECT_RATIO);
    }

    @Test
    public void testScaling_bothUnspecified() {
        mView.setImageDrawable(mDrawable);

        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mView.measure(widthSpec, heightSpec);

        assertThat(mView.getMeasuredHeight()).isEqualTo(0xFFFFFF);
        assertThat(mView.getMeasuredWidth()).isEqualTo(0xFFFFFF);
    }
}
