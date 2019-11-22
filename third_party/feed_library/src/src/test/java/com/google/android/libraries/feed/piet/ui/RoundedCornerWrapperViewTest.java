// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Build.VERSION_CODES;

import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.piet.ui.RoundedCornerDelegateFactory.RoundingStrategy;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.Borders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for the {@link RoundedCornerWrapperView}. */
@RunWith(RobolectricTestRunner.class)
public class RoundedCornerWrapperViewTest {
    private static final int TOP_CORNERS_BITMASK = 3;
    private static final int LEFT_CORNERS_BITMASK = 9;
    private static final int ALL_CORNERS_BITMASK = 15;

    private Canvas canvas;
    private Bitmap bitmap;

    @Before
    public void setUp() {
        bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        canvas = new Canvas(bitmap);
    }

    @Test
    public void layoutZeroDimensions_notRounded() {
        RoundedCornerWrapperView view = CommonRoundedCornerWrapperView.getDefaultInstance();

        view.layout(0, 0, 0, 100);
        assertThat(view.getWidth()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails

        view.layout(0, 0, 100, 0);
        assertThat(view.getHeight()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails

        view.layout(0, 0, 0, 0);
        assertThat(view.getWidth()).isEqualTo(0);
        assertThat(view.getHeight()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails
    }

    @Test
    public void layoutZeroDimensions_rounded() {
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setBitmask(7).setRadiusDp(10).build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        view.layout(0, 0, 0, 100);
        assertThat(view.getWidth()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails

        view.layout(0, 0, 100, 0);
        assertThat(view.getHeight()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails

        view.layout(0, 0, 0, 0);
        assertThat(view.getWidth()).isEqualTo(0);
        assertThat(view.getHeight()).isEqualTo(0);
        view.draw(canvas);
        // Assert nothing fails
    }

    @Test
    public void roundCorners_bordersAdded() {
        // The width, height, and radius don't really matter for this test. As long as they exist, a
        // BorderDrawable should be set on the view.
        Borders borders = Borders.newBuilder().setWidth(10).build();

        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                .setRoundedCorners(roundedCorners)
                                                .setBorders(borders)
                                                .build();

        view.layout(0, 0, 100, 100);
        view.draw(canvas);

        // Assert that borders are created.
        assertThat(view.getForeground()).isInstanceOf(BorderDrawable.class);
        // The specifics of how the borders look are difficult to test here, and so are tested in a
        // screendiff test instead.
    }

    @Test
    public void setRoundedCorners_bordersWidthZero() {
        Borders borders = Borders.newBuilder().setWidth(0).build();

        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                .setRoundedCorners(roundedCorners)
                                                .setBorders(borders)
                                                .build();

        // Set a width and height on the view so that the only thing stopping borders from being
        // created is the border width of 0.
        view.layout(0, 0, 100, 100);
        view.draw(canvas);

        // Assert that borders are not created.
        assertThat(view.getForeground()).isNull();
    }

    @Test
    public void getRadius_usesOverride() {
        int viewWidth = 100;
        int viewHeight = 100;
        int radiusToSet = 10;
        int radiusOverride = 20;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusDp(radiusToSet).build();
        RoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                .setRoundedCorners(roundedCorners)
                                                .setRadiusOverride(radiusOverride)
                                                .build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Make sure it's using the override value, NOT the radius from RoundedCorners.
        assertThat(calculatedRadius).isEqualTo(radiusOverride);
    }

    @Test
    public void getRadius_radiusDp() {
        int viewWidth = 100;
        int viewHeight = 100;
        int radiusToSet = 10;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusDp(radiusToSet).build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        assertThat(calculatedRadius).isEqualTo(radiusToSet);
    }

    @Test
    public void getRadius_radiusPercentageOfHeight() {
        int viewWidth = 60;
        int viewHeight = 100;
        int radiusPercentageOfHeight = 25;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder()
                        .setRadiusPercentageOfHeight(radiusPercentageOfHeight)
                        .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // 25% of the height is 25. .25 * 100 = 25
        assertThat(calculatedRadius).isEqualTo(25);
    }

    @Test
    public void getRadius_radiusPercentageOfWidth() {
        int viewWidth = 60;
        int viewHeight = 100;
        int radiusPercentageOfWidth = 25;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setRadiusPercentageOfWidth(radiusPercentageOfWidth)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // 25% of the width is 15. .25 * 60 = 15
        assertThat(calculatedRadius).isEqualTo(15);
    }

    @Test
    public void getRadius_smallWidth_RadiusDpShouldAdjust() {
        int viewWidth = 30;
        int viewHeight = 40;
        int radiusToSet = 20;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusDp(radiusToSet).build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // A radius of 20 on each corner will add up to 40, which is bigger than the width. The
        // radius should be adjusted to be 15.
        assertThat(calculatedRadius).isEqualTo(15);
    }

    @Test
    public void getRadius_smallWidth_RadiusDpShouldNotAdjust() {
        int viewWidth = 30;
        int viewHeight = 40;
        int radiusToSet = 20;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(LEFT_CORNERS_BITMASK)
                                                .setRadiusDp(radiusToSet)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // The radius of 20 is more than half the width. However, since only the left corners are
        // rounded, the radius should not be adjusted, since 20dp is only half the height.
        assertThat(calculatedRadius).isEqualTo(20);
    }

    @Test
    public void getRadius_smallHeight_RadiusDpShouldAdjust() {
        int viewWidth = 40;
        int viewHeight = 30;
        int radiusToSet = 20;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusDp(radiusToSet).build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // A radius of 20 on each corner will add up to 40, which is bigger than the height. The
        // radius should be adjusted to be 15.
        assertThat(calculatedRadius).isEqualTo(15);
    }

    @Test
    public void getRadius_smallHeight_RadiusDpShouldNotAdjust() {
        int viewWidth = 40;
        int viewHeight = 30;
        int radiusToSet = 20;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(TOP_CORNERS_BITMASK)
                                                .setRadiusDp(radiusToSet)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // The radius of 20 is more than half the height. However, since only the top corners are
        // rounded, the radius should not be adjusted, since 20dp is only half the width.
        assertThat(calculatedRadius).isEqualTo(20);
    }

    @Test
    public void getRadius_radiusPercentageOfHeight_100Percent_shouldAdjust() {
        int viewWidth = 60;
        int viewHeight = 100;
        int radiusPercentageOfHeight = 100;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder()
                        .setRadiusPercentageOfHeight(radiusPercentageOfHeight)
                        .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Since all corners are rounded, the radius should shrink to 30, half the width.
        assertThat(calculatedRadius).isEqualTo(30);
    }

    @Test
    public void getRadius_radiusPercentageOfHeight_100Percent_topCorners_shouldAdjust() {
        int viewWidth = 60;
        int viewHeight = 100;
        int radiusPercentageOfHeight = 100;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder()
                        .setBitmask(TOP_CORNERS_BITMASK)
                        .setRadiusPercentageOfHeight(radiusPercentageOfHeight)
                        .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Only top corners are rounded, but the radius should still shrink to 30, half the width,
        // since the width is small.
        assertThat(calculatedRadius).isEqualTo(30);
    }

    @Test
    public void getRadius_radiusPercentageOfHeight_100Percent_topCorners_shouldNotAdjust() {
        int viewWidth = 100;
        int viewHeight = 40;
        int radiusPercentageOfHeight = 100;
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder()
                        .setBitmask(TOP_CORNERS_BITMASK)
                        .setRadiusPercentageOfHeight(radiusPercentageOfHeight)
                        .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Only top corners are rounded, and 40*2=80 is still less than the width (100), so we don't
        // need to shrink the radius.
        assertThat(calculatedRadius).isEqualTo(40);
    }

    @Test
    public void getRadius_radiusPercentageOfWidth_100Percent_shouldAdjust() {
        int viewWidth = 100;
        int viewHeight = 60;
        int radiusPercentageOfWidth = 100;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setRadiusPercentageOfWidth(radiusPercentageOfWidth)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Since all corners are rounded, the radius should shrink to 30, half the height, instead
        // of 100% of 100 = 100.
        assertThat(calculatedRadius).isEqualTo(30);
    }

    @Test
    public void getRadius_radiusPercentageOfWidth_100Percent_leftCorners_shouldAdjust() {
        int viewWidth = 100;
        int viewHeight = 60;
        int radiusPercentageOfWidth = 100;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(LEFT_CORNERS_BITMASK)
                                                .setRadiusPercentageOfWidth(radiusPercentageOfWidth)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Only left corners are rounded, but the radius should still shrink to 30, half the height,
        // since the height is small. 100% of 100 = 100, which is greater than the height.
        assertThat(calculatedRadius).isEqualTo(30);
    }

    @Test
    public void getRadius_radiusPercentageOfWidth_100Percent_topCorners_shouldNotAdjust() {
        int viewWidth = 40;
        int viewHeight = 100;
        int radiusPercentageOfWidth = 100;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(LEFT_CORNERS_BITMASK)
                                                .setRadiusPercentageOfWidth(radiusPercentageOfWidth)
                                                .build();
        RoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        int calculatedRadius = view.getRadius(viewWidth, viewHeight);

        // Only left corners are rounded, and 40*2=80 is still less than the height (100), so we
        // don't need to shrink the radius.
        assertThat(calculatedRadius).isEqualTo(40);
    }

    @Test
    public void useMaskingStrategy_notAllCornersRounded() {
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(LEFT_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        // When not all corners are rounded, the outline provider strategy doesn't work, so it
        // should use bitmap masking
        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.BITMAP_MASKING);
    }

    @Test
    public void useOutlineProviderStrategy_hardwareAcceleration() {
        int width = 100;
        int height = 100;
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setHardwareAccelerated(true)
                                                      .build();

        // Lay out the view, because that's when the view would know if it's hardware accelerated.
        view.layout(0, 0, width, height);

        assertThat(view.getClipToOutline()).isTrue();
        assertThat(view.getClipChildren()).isTrue();
        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.OUTLINE_PROVIDER);
    }

    @Config(sdk = VERSION_CODES.KITKAT)
    @Test
    public void useClipPathStrategy_kitkat() {
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view =
                CommonRoundedCornerWrapperView.builder().setRoundedCorners(roundedCorners).build();

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.CLIP_PATH);
    }

    @Config(sdk = VERSION_CODES.KITKAT)
    @Test
    public void useMaskingStrategy_kitkat_noClipPathAllowed() {
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setAllowClipPath(false)
                                                      .build();

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.BITMAP_MASKING);
    }

    @Test
    public void useClipPathStrategy_noHardwareAcceleration() {
        int width = 100;
        int height = 100;
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setHardwareAccelerated(false)
                                                      .build();

        view.layout(0, 0, width, height);

        assertThat(view.getClipToOutline()).isFalse();
        assertThat(view.getClipChildren()).isFalse();

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.CLIP_PATH);
    }

    @Test
    public void useMaskingStrategy_noHardwareAcceleration_noClipPathAllowed() {
        int width = 100;
        int height = 100;
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setHardwareAccelerated(false)
                                                      .setAllowClipPath(false)
                                                      .build();

        view.layout(0, 0, width, height);

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.BITMAP_MASKING);
    }

    @Test
    public void useMaskingStrategy_withHwAcceleration_noClipPathAllowed_noOutlineProviderAllowed() {
        int width = 100;
        int height = 100;
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setHardwareAccelerated(true)
                                                      .setAllowClipPath(false)
                                                      .setAllowOutlineRounding(false)
                                                      .build();

        view.layout(0, 0, width, height);

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.BITMAP_MASKING);
    }

    @Test
    public void allowAllRoundingStrategies_shouldChooseOutlineProvider() {
        int width = 100;
        int height = 100;
        int radius = 16;
        RoundedCorners roundedCorners = RoundedCorners.newBuilder()
                                                .setBitmask(ALL_CORNERS_BITMASK)
                                                .setRadiusDp(radius)
                                                .build();
        CommonRoundedCornerWrapperView view = CommonRoundedCornerWrapperView.builder()
                                                      .setRoundedCorners(roundedCorners)
                                                      .setHardwareAccelerated(true)
                                                      .setAllowClipPath(true)
                                                      .setAllowOutlineRounding(true)
                                                      .build();

        view.layout(0, 0, width, height);

        assertThat(view.getClipToOutline()).isTrue();
        assertThat(view.getClipChildren()).isTrue();

        assertThat(view.getRoundingStrategy()).isEqualTo(RoundingStrategy.OUTLINE_PROVIDER);
    }

    static class CommonRoundedCornerWrapperView extends RoundedCornerWrapperView {
        private static final Supplier<Boolean> IS_RTL_SUPPLIER = Suppliers.of(false);
        private static final Context CONTEXT = Robolectric.buildActivity(Activity.class).get();
        private final boolean hardwareAccelerated;
        private RoundingStrategy roundingStrategy;

        @Override
        public boolean isHardwareAccelerated() {
            return hardwareAccelerated;
        }

        @Override
        public boolean isAttachedToWindow() {
            return true;
        }

        @Override
        void setRoundingDelegate(RoundingStrategy roundingStrategy) {
            super.setRoundingDelegate(roundingStrategy);
            this.roundingStrategy = roundingStrategy;
        }

        RoundingStrategy getRoundingStrategy() {
            return roundingStrategy;
        }

        private CommonRoundedCornerWrapperView(Builder builder) {
            super(CONTEXT, builder.roundedCorners, new RoundedCornerMaskCache(), IS_RTL_SUPPLIER,
                    builder.radiusOverride, builder.borders, builder.allowClipPath,
                    builder.allowOutlineRounding);
            this.hardwareAccelerated = builder.hardwareAccelerated;
        }

        static CommonRoundedCornerWrapperView getDefaultInstance() {
            return new CommonRoundedCornerWrapperView(new CommonRoundedCornerWrapperView.Builder());
        }

        static Builder builder() {
            return new CommonRoundedCornerWrapperView.Builder();
        }

        static class Builder {
            private boolean hardwareAccelerated;

            RoundedCorners roundedCorners = RoundedCorners.getDefaultInstance();
            int radiusOverride = 0;
            Borders borders = Borders.getDefaultInstance();
            boolean allowClipPath = true;
            boolean allowOutlineRounding = true;

            Builder setHardwareAccelerated(boolean value) {
                hardwareAccelerated = value;
                return this;
            }

            Builder setRoundedCorners(RoundedCorners value) {
                roundedCorners = value;
                return this;
            }

            Builder setRadiusOverride(int value) {
                radiusOverride = value;
                return this;
            }

            Builder setBorders(Borders value) {
                borders = value;
                return this;
            }

            Builder setAllowClipPath(boolean value) {
                allowClipPath = value;
                return this;
            }

            Builder setAllowOutlineRounding(boolean value) {
                allowOutlineRounding = value;
                return this;
            }

            CommonRoundedCornerWrapperView build() {
                return new CommonRoundedCornerWrapperView(this);
            }
        }
    }
}
