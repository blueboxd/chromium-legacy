// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import static com.google.android.libraries.feed.common.Validators.checkState;

import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.ShapeDrawable;
import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.common.functional.Supplier;

/** Generates a linear gradient according to CSS behavior */
class GradientShader extends ShapeDrawable.ShaderFactory {
    private final int[] colors;
    private final float[] stops;
    @VisibleForTesting
    final double angleRadians;
    @VisibleForTesting
    final double angleRadiansSmall;
    @VisibleForTesting /*@Nullable*/ final Supplier<Boolean> rtLSupplier;

    GradientShader(
            int[] colors, float[] stops, int angle, /*@Nullable*/ Supplier<Boolean> rtLSupplier) {
        checkState(colors.length == stops.length, "Mismatch: got %s colors and %s stops",
                colors.length, stops.length);
        this.colors = colors;
        this.stops = stops;
        this.angleRadians = Math.toRadians(angle % 360);
        this.angleRadiansSmall = Math.toRadians(angle % 90);
        this.rtLSupplier = rtLSupplier;
    }

    @Override
    public Shader resize(int width, int height) {
        RectF line = calculateGradientLine(width, height, angleRadians, angleRadiansSmall, isRtL());

        return new android.graphics.LinearGradient(
                line.left, line.top, line.right, line.bottom, colors, stops, TileMode.CLAMP);
    }

    private boolean isRtL() {
        return rtLSupplier != null && rtLSupplier.get();
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
