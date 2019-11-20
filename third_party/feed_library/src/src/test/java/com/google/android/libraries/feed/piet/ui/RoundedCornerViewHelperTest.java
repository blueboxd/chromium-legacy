// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for the {@link RoundedCornerViewHelper}. */
@RunWith(RobolectricTestRunner.class)
public class RoundedCornerViewHelperTest {

  @Test
  public void hasNoValidRoundedCorners_noRadiusSet() {
    RoundedCorners roundedCorners = RoundedCorners.getDefaultInstance();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 0);

    assertThat(calculatedValidity).isFalse();
  }

  @Test
  public void hasNoValidRoundedCorners_zeroRadius() {
    RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(0).build();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 0);

    assertThat(calculatedValidity).isFalse();
  }

  @Test
  public void hasValidRoundedCorners_radiusDp() {
    RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 0);

    assertThat(calculatedValidity).isTrue();
  }

  @Test
  public void hasValidRoundedCorners_radiusPercentageOfHeight() {
    RoundedCorners roundedCorners =
        RoundedCorners.newBuilder().setRadiusPercentageOfHeight(20).build();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 0);

    assertThat(calculatedValidity).isTrue();
  }

  @Test
  public void hasValidRoundedCorners_radiusPercentageOfWidth() {
    RoundedCorners roundedCorners =
        RoundedCorners.newBuilder().setRadiusPercentageOfWidth(30).build();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 0);

    assertThat(calculatedValidity).isTrue();
  }

  @Test
  public void hasValidRoundedCorners_radiusOverride() {
    RoundedCorners roundedCorners = RoundedCorners.getDefaultInstance();

    boolean calculatedValidity =
        RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, /* radiusOverride= */ 10);

    assertThat(calculatedValidity).isTrue();
  }
}
