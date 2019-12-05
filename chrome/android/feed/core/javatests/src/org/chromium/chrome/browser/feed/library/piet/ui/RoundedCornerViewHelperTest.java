// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link RoundedCornerViewHelper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RoundedCornerViewHelperTest {
    @Test
    public void hasNoValidRoundedCorners_noRadiusSet() {
        RoundedCorners roundedCorners = RoundedCorners.getDefaultInstance();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 0);

        assertThat(calculatedValidity).isFalse();
    }

    @Test
    public void hasNoValidRoundedCorners_zeroRadius() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(0).build();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 0);

        assertThat(calculatedValidity).isFalse();
    }

    @Test
    public void hasValidRoundedCorners_radiusDp() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 0);

        assertThat(calculatedValidity).isTrue();
    }

    @Test
    public void hasValidRoundedCorners_radiusPercentageOfHeight() {
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusPercentageOfHeight(20).build();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 0);

        assertThat(calculatedValidity).isTrue();
    }

    @Test
    public void hasValidRoundedCorners_radiusPercentageOfWidth() {
        RoundedCorners roundedCorners =
                RoundedCorners.newBuilder().setRadiusPercentageOfWidth(30).build();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 0);

        assertThat(calculatedValidity).isTrue();
    }

    @Test
    public void hasValidRoundedCorners_radiusOverride() {
        RoundedCorners roundedCorners = RoundedCorners.getDefaultInstance();

        boolean calculatedValidity = RoundedCornerViewHelper.hasValidRoundedCorners(
                roundedCorners, /* radiusOverride= */ 10);

        assertThat(calculatedValidity).isTrue();
    }
}
