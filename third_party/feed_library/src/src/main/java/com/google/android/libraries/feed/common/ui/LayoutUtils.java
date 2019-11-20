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

package com.google.android.libraries.feed.common.ui;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import java.util.Locale;

/** General utilities to help with UI layout. */
public class LayoutUtils {
  @TargetApi(VERSION_CODES.JELLY_BEAN_MR1)
  public static void setMarginsRelative(
      MarginLayoutParams params, int start, int top, int end, int bottom) {
    params.setMargins(start, top, end, bottom);
    if (Build.VERSION.SDK_INT >= VERSION_CODES.JELLY_BEAN_MR1) {
      params.setMarginStart(start);
      params.setMarginEnd(end);
    }
  }

  /**
   * Converts DP to PX, where PX represents the actual number of pixels displayed, based on the
   * density of the phone screen. DP represents density-independent pixels, which are always the
   * same size, regardless of density.
   */
  public static float dpToPx(float dp, Context context) {
    DisplayMetrics metrics = context.getResources().getDisplayMetrics();
    return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, metrics);
  }

  /**
   * Converts SP to PX, where SP represents scale-independent pixels (a value that scales with
   * accessibility settings), and PX represents the actual number of pixels displayed, based on the
   * density of the phone screen.
   */
  public static float spToPx(float sp, Context context) {
    DisplayMetrics metrics = context.getResources().getDisplayMetrics();
    return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, sp, metrics);
  }

  /**
   * Converts PX to SP, where SP represents scale-independent pixels (a value that scales with
   * accessibility settings), and PX represents the actual number of pixels displayed, based on the
   * density of the phone screen.
   */
  public static float pxToSp(float px, Context context) {
    DisplayMetrics metrics = context.getResources().getDisplayMetrics();
    return px / metrics.scaledDensity;
  }

  /**
   * Converts PX to DP, where PX represents the actual number of pixels displayed, based on the
   * density of the phone screen. DP represents density-independent pixels, which are always the
   * same size, regardless of density.
   */
  public static float pxToDp(float px, Context context) {
    DisplayMetrics metrics = context.getResources().getDisplayMetrics();
    return px / metrics.density;
  }

  /** Determines whether current locale is RTL. */
  @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
  public static boolean isDefaultLocaleRtl() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
      return View.LAYOUT_DIRECTION_RTL
          == TextUtils.getLayoutDirectionFromLocale(Locale.getDefault());
    } else {
      return false;
    }
  }
}
