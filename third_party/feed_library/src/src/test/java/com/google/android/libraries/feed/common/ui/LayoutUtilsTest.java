// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.ui;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.Build.VERSION_CODES;
import android.widget.LinearLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link LayoutUtils}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.JELLY_BEAN_MR1)
public class LayoutUtilsTest {
    private static final int START = 1;
    private static final int TOP = 2;
    private static final int END = 3;
    private static final int BOTTOM = 4;

    private final Context context = ApplicationProvider.getApplicationContext();

    @Test
    @Config(sdk = VERSION_CODES.JELLY_BEAN)
    public void testSetMarginsRelative_api16() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        LayoutUtils.setMarginsRelative(lp, START, TOP, END, BOTTOM);
        assertThat(lp.leftMargin).isEqualTo(START);
        assertThat(lp.rightMargin).isEqualTo(END);
        assertThat(lp.topMargin).isEqualTo(TOP);
        assertThat(lp.bottomMargin).isEqualTo(BOTTOM);
    }

    @Test
    @Config(sdk = VERSION_CODES.JELLY_BEAN_MR1)
    public void testSetMarginsRelative_api17() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        LayoutUtils.setMarginsRelative(lp, START, TOP, END, BOTTOM);
        assertThat(lp.getMarginStart()).isEqualTo(START);
        assertThat(lp.getMarginEnd()).isEqualTo(END);
        assertThat(lp.topMargin).isEqualTo(TOP);
        assertThat(lp.bottomMargin).isEqualTo(BOTTOM);
    }

    @Test
    public void testDpToPx() {
        // TODO: Switch this to using @Config to set density to something different than 1.
        assertThat(LayoutUtils.dpToPx(1000.0f, context)).isWithin(1.0e-04f).of(1000.0f);
    }

    @Test
    public void testPxToDp() {
        // TODO: Switch this to using @Config to set density to something different than 1.
        assertThat(LayoutUtils.pxToDp(1000.0f, context)).isWithin(1.0e-04f).of(1000.0f);
    }

    @Test
    public void testSpToPx() {
        context.getResources().getDisplayMetrics().scaledDensity = 3.0f;
        assertThat(LayoutUtils.spToPx(1000.0f, context)).isWithin(1.0e-03f).of(3000.0f);
    }

    @Test
    public void testPxToSp() {
        context.getResources().getDisplayMetrics().scaledDensity = 3.0f;
        assertThat(LayoutUtils.pxToSp(3000.0f, context)).isWithin(1.0e-04f).of(1000.0f);
    }
}
