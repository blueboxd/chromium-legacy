// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.support.v4.widget.CircularProgressDrawable;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link MaterialSpinnerView}. */
@RunWith(RobolectricTestRunner.class)
public class MaterialSpinnerViewTest {
    private MaterialSpinnerView materialSpinnerView;

    @Before
    public void setUp() {
        Activity context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        materialSpinnerView = new MaterialSpinnerView(context);
    }

    @Test
    public void testInit_isVisible_spinnerStarted() {
        assertThat(materialSpinnerView.getVisibility()).isEqualTo(View.VISIBLE);

        assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning())
                .isTrue();
    }

    @Test
    public void testSetVisibility_gone_stopsSpinner() {
        materialSpinnerView.setVisibility(View.GONE);

        assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning())
                .isFalse();
    }

    @Test
    public void testSetVisibility_invisible_stopsSpinner() {
        materialSpinnerView.setVisibility(View.INVISIBLE);

        assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning())
                .isFalse();
    }

    @Test
    public void testSetVisibility_toTrue_startsSpinner() {
        materialSpinnerView.setVisibility(View.GONE);
        materialSpinnerView.setVisibility(View.VISIBLE);

        assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning())
                .isTrue();
    }
}
