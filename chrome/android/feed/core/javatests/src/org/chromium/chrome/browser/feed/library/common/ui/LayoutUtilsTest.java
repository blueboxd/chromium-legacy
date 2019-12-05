// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.ui;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.widget.LinearLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link LayoutUtils}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LayoutUtilsTest {
    private static final int START = 1;
    private static final int TOP = 2;
    private static final int END = 3;
    private static final int BOTTOM = 4;

    private final Context mContext = ApplicationProvider.getApplicationContext();

    @Test
    public void testSetMarginsRelative() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        LayoutUtils.setMarginsRelative(lp, START, TOP, END, BOTTOM);
        assertThat(lp.leftMargin).isEqualTo(START);
        assertThat(lp.rightMargin).isEqualTo(END);
        assertThat(lp.topMargin).isEqualTo(TOP);
        assertThat(lp.bottomMargin).isEqualTo(BOTTOM);
    }

    @Test
    public void testDpToPx() {
        // TODO: Switch this to using @Config to set density to something different than 1.
        assertThat(LayoutUtils.dpToPx(1000.0f, mContext)).isWithin(1.0e-04f).of(1000.0f);
    }

    @Test
    public void testPxToDp() {
        // TODO: Switch this to using @Config to set density to something different than 1.
        assertThat(LayoutUtils.pxToDp(1000.0f, mContext)).isWithin(1.0e-04f).of(1000.0f);
    }

    @Test
    public void testSpToPx() {
        mContext.getResources().getDisplayMetrics().scaledDensity = 3.0f;
        assertThat(LayoutUtils.spToPx(1000.0f, mContext)).isWithin(1.0e-03f).of(3000.0f);
    }

    @Test
    public void testPxToSp() {
        mContext.getResources().getDisplayMetrics().scaledDensity = 3.0f;
        assertThat(LayoutUtils.pxToSp(3000.0f, mContext)).isWithin(1.0e-04f).of(1000.0f);
    }
}
