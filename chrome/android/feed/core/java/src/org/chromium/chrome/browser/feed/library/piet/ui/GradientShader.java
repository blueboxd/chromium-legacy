// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.ShapeDrawable;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;

/** Generates a linear gradient according to CSS behavior */
class GradientShader extends ShapeDrawable.ShaderFactory {
    private final int[] mColors;
    private final float[] mStops;
    @VisibleForTesting
    final double mAngleRadians;
    @VisibleForTesting
    final double mAngleRadiansSmall;
    @VisibleForTesting
    @Nullable
    final Supplier<Boolean> mRtLSupplier;

    GradientShader(
            int[] colors, float[] stops, int angle, @Nullable Supplier<Boolean> rtLSupplier) {
        checkState(colors.length == stops.length, "Mismatch: got %s colors and %s stops",
                colors.length, stops.length);
        this.mColors = colors;
        this.mStops = stops;
        this.mAngleRadians = Math.toRadians(angle % 360);
        this.mAngleRadiansSmall = Math.toRadians(angle % 90);
        this.mRtLSupplier = rtLSupplier;
    }

    @Override
    public Shader resize(int width, int height) {
        RectF line =
                calculateGradientLine(width, height, mAngleRadians, mAngleRadiansSmall, isRtL());

        return new android.graphics.LinearGradient(
                line.left, line.top, line.right, line.bottom, mColors, mStops, TileMode.CLAMP);
    }

    private boolean isRtL() {
        return mRtLSupplier != null && mRtLSupplier.get();
    }

    /** Returns a RectF as a shorthand for two points - start coords and end coords */
    @VisibleForTesting
    static RectF calculateGradientLine(
            int width, int height, double angleRadians, double angleRadiansSmall, boolean isRtL) {
        // See documentation on how CSS gradients work at:
        // https://developer.mozilla.org/en-US/docs/Web/CSS/linear-gradient

        // This is Math. It's kind of like Magic.
        // You put numbers in, and the right gradients come out.
        // Note that on Android, the Y axis starts at the top and increases to the bottom, so some
        // of the math is backward.
        double length = Math.hypot(width / 2.0d, height / 2.0d)
                * Math.cos((Math.PI / 2) - Math.atan2(height, width) - angleRadiansSmall);
        float rtLmultiplier = isRtL ? -1 : 1;
        float startX = (float) ((width / 2.0f) - length * rtLmultiplier * Math.sin(angleRadians));
        float endX = (float) ((width / 2.0f) + length * rtLmultiplier * Math.sin(angleRadians));
        float startY = (float) ((height / 2.0f) + length * Math.cos(angleRadians));
        float endY = (float) ((height / 2.0f) - length * Math.cos(angleRadians));

        return new RectF(startX, startY, endX, endY);
    }
}
