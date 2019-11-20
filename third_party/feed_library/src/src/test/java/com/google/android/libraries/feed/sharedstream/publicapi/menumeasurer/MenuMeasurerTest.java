// Copyright 2018 The Feed Authors.
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

package com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer;

import static com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.MenuMeasurer.NO_MAX_HEIGHT;
import static com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.MenuMeasurer.NO_MAX_WIDTH;
import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import com.google.common.collect.Lists;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link MenuMeasurer}. */
@RunWith(RobolectricTestRunner.class)
public class MenuMeasurerTest {

  private static final int NO_PADDING = 0;

  private MenuMeasurer menuMeasurer;
  private int widthUnit;

  @Before
  public void setup() {
    Activity activity = Robolectric.buildActivity(Activity.class).create().visible().get();
    menuMeasurer = new MenuMeasurer(activity);
    widthUnit = activity.getResources().getDimensionPixelSize(R.dimen.menu_width_multiple);
  }

  @Test
  public void testCalculateSize() {
    assertThat(
            menuMeasurer.calculateSize(
                Lists.newArrayList(new Size(10, 20), new Size(10, 20)),
                NO_PADDING,
                NO_MAX_WIDTH,
                NO_MAX_HEIGHT))
        .isEqualTo(new Size(roundToWidthUnit(10), 40));
  }

  @Test
  public void testCalculateSize_maxWidth() {
    assertThat(
            menuMeasurer.calculateSize(
                Lists.newArrayList(new Size(100, 100)), NO_PADDING, 70, NO_MAX_HEIGHT))
        .isEqualTo(new Size(70, 100));
  }

  @Test
  public void testCalculateSize_maxHeight() {
    assertThat(
            menuMeasurer.calculateSize(
                Lists.newArrayList(new Size(100, 100)), NO_PADDING, NO_MAX_WIDTH, 60))
        .isEqualTo(new Size(roundToWidthUnit(100), 60));
  }

  private int roundToWidthUnit(int measuredWidth) {
    return Math.round((((float) measuredWidth / (float) widthUnit) + 0.5f)) * widthUnit;
  }
}
