// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.ui;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import java.util.Locale;

/** General utilities to help with UI layout. */
public class LayoutUtils {
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    public static void setMarginsRelative(
            MarginLayoutParams params, int start, int top, int end, int bottom) {
        params.setMargins(start, top, end, bottom);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
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
     * accessibility settings), and PX represents the actual number of pixels displayed, based on
     * the density of the phone screen.
     */
    public static float spToPx(float sp, Context context) {
        DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, sp, metrics);
    }

    /**
     * Converts PX to SP, where SP represents scale-independent pixels (a value that scales with
     * accessibility settings), and PX represents the actual number of pixels displayed, based on
     * the density of the phone screen.
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
