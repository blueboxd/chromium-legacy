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

package com.google.android.libraries.feed.sharedstream.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.view.View;
import android.support.v4.widget.CircularProgressDrawable;
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

    assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning()).isTrue();
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

    assertThat(((CircularProgressDrawable) materialSpinnerView.getDrawable()).isRunning()).isTrue();
  }
}
