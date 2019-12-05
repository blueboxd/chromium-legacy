// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.Color;
import android.graphics.drawable.shapes.RectShape;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.ColorStop;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.LinearGradient;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link GradientDrawable}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GradientDrawableTest {
    @Test
    public void testCreatesShader() {
        GradientDrawable drawable = new GradientDrawable(
                LinearGradient.newBuilder()
                        .setDirectionDeg(123)
                        .addStops(ColorStop.newBuilder().setPositionPercent(0).setColor(Color.BLUE))
                        .addStops(
                                ColorStop.newBuilder().setPositionPercent(100).setColor(Color.RED))
                        .build(),
                Suppliers.of(true));
        assertThat(drawable.getShape()).isInstanceOf(RectShape.class);
        assertThat(drawable.getShaderFactory()).isInstanceOf(GradientShader.class);
    }

    @Test
    public void testCreatesShader_noRtlSupport() {
        GradientDrawable drawable = new GradientDrawable(
                LinearGradient.newBuilder()
                        .setDirectionDeg(123)
                        .addStops(ColorStop.newBuilder().setPositionPercent(0).setColor(Color.BLUE))
                        .addStops(
                                ColorStop.newBuilder().setPositionPercent(100).setColor(Color.RED))
                        .build(),
                Suppliers.of(true));

        assertThat(((GradientShader) drawable.getShaderFactory()).mRtLSupplier).isNull();
    }

    @Test
    public void testCreatesShader_rtl() {
        GradientDrawable drawable = new GradientDrawable(
                LinearGradient.newBuilder()
                        .setDirectionDeg(123)
                        .addStops(ColorStop.newBuilder().setPositionPercent(0).setColor(Color.BLUE))
                        .addStops(
                                ColorStop.newBuilder().setPositionPercent(100).setColor(Color.RED))
                        .setReverseForRtl(true)
                        .build(),
                Suppliers.of(true));

        assertThat(((GradientShader) drawable.getShaderFactory()).mRtLSupplier.get()).isTrue();
    }

    @Test
    public void testCreatesShader_ltr() {
        GradientDrawable drawable = new GradientDrawable(
                LinearGradient.newBuilder()
                        .setDirectionDeg(123)
                        .addStops(ColorStop.newBuilder().setPositionPercent(0).setColor(Color.BLUE))
                        .addStops(
                                ColorStop.newBuilder().setPositionPercent(100).setColor(Color.RED))
                        .setReverseForRtl(true)
                        .build(),
                Suppliers.of(false));

        assertThat(((GradientShader) drawable.getShaderFactory()).mRtLSupplier.get()).isFalse();
    }
}
