// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.MenuMeasurer.NO_MAX_HEIGHT;
import static org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.MenuMeasurer.NO_MAX_WIDTH;

import android.app.Activity;

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link MenuMeasurer}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MenuMeasurerTest {
    private static final int NO_PADDING = 0;

    private MenuMeasurer mMenuMeasurer;
    private int mWidthUnit;

    @Before
    public void setup() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().visible().get();
        mMenuMeasurer = new MenuMeasurer(activity);
        mWidthUnit = activity.getResources().getDimensionPixelSize(R.dimen.menu_width_multiple);
    }

    @Test
    public void testCalculateSize() {
        assertThat(
                mMenuMeasurer.calculateSize(Lists.newArrayList(new Size(10, 20), new Size(10, 20)),
                        NO_PADDING, NO_MAX_WIDTH, NO_MAX_HEIGHT))
                .isEqualTo(new Size(roundToWidthUnit(10), 40));
    }

    @Test
    public void testCalculateSize_maxWidth() {
        assertThat(mMenuMeasurer.calculateSize(
                           Lists.newArrayList(new Size(100, 100)), NO_PADDING, 70, NO_MAX_HEIGHT))
                .isEqualTo(new Size(70, 100));
    }

    @Test
    public void testCalculateSize_maxHeight() {
        assertThat(mMenuMeasurer.calculateSize(
                           Lists.newArrayList(new Size(100, 100)), NO_PADDING, NO_MAX_WIDTH, 60))
                .isEqualTo(new Size(roundToWidthUnit(100), 60));
    }

    private int roundToWidthUnit(int measuredWidth) {
        return Math.round((((float) measuredWidth / (float) mWidthUnit) + 0.5f)) * mWidthUnit;
    }
}
