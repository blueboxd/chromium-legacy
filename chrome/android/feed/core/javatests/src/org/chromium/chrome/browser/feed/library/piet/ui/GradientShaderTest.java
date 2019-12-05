// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.graphics.Color;
import android.graphics.RectF;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for the {@link GradientShader}.
 *
 * <p>Note that on Android, the Y axis starts at the top and increases to the bottom, so some of the
 * math is backward.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GradientShaderTest {
    private static final boolean RTL = true;
    private static final boolean LTR = false;

    @Test
    public void testGradientLine_leftToRight() {
        RectF points = GradientShader.calculateGradientLine(12, 12, Math.toRadians(90), 0, LTR);
        checkGradientLine(points, 0, 6, 12, 6);
    }

    @Test
    public void testGradientLine_bottomToTop() {
        RectF points = GradientShader.calculateGradientLine(12, 12, 0, 0, LTR);
        checkGradientLine(points, 6, 12, 6, 0);
    }

    @Test
    public void testGradientLine_bottomLeftToTopRight() {
        RectF points = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(45), Math.toRadians(45), LTR);
        checkGradientLine(points, 0, 12, 12, 0);
    }

    @Test
    public void testGradientLine_bottomRightToTopLeft() {
        RectF points = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(315), Math.toRadians(315 % 90), LTR);
        checkGradientLine(points, 12, 12, 0, 0);
    }

    @Test
    public void testGradientLine_thirtyDegrees() {
        RectF points = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(30), Math.toRadians(30), LTR);
        checkGradientLine(points, 1.9019f, 13.0981f, 10.0981f, -1.0981f);
    }

    @Test
    public void testGradientLine_threeThirtyDegrees() {
        RectF points = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(330), Math.toRadians(330 % 90), LTR);
        checkGradientLine(points, 10.0981f, 13.0981f, 1.9019f, -1.0981f);
    }

    @Test
    public void testGradientLine_horizontalRtL() {
        RectF points = GradientShader.calculateGradientLine(12, 12, Math.toRadians(90), 0, RTL);
        checkGradientLine(points, 12, 6, 0, 6);

        // Flip the angle 180; should be the same as the RtL version
        RectF pointsLtR = GradientShader.calculateGradientLine(12, 12, Math.toRadians(270), 0, LTR);
        checkGradientLine(points, pointsLtR.left, pointsLtR.top, pointsLtR.right, pointsLtR.bottom);
    }

    @Test
    public void testGradientLine_verticalRtL() {
        RectF points = GradientShader.calculateGradientLine(12, 12, 0, 0, LTR);
        checkGradientLine(points, 6, 12, 6, 0);

        // RtL should not change the points for a vertical line
        RectF pointsLtR = GradientShader.calculateGradientLine(12, 12, 0, 0, LTR);
        checkGradientLine(points, pointsLtR.left, pointsLtR.top, pointsLtR.right, pointsLtR.bottom);
    }

    @Test
    public void testGradientLine_angleRtL() {
        RectF points = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(45), Math.toRadians(45), RTL);
        checkGradientLine(points, 12, 12, 0, 0);

        RectF pointsLtR = GradientShader.calculateGradientLine(
                12, 12, Math.toRadians(360 - 45), Math.toRadians(45), LTR);
        checkGradientLine(points, pointsLtR.left, pointsLtR.top, pointsLtR.right, pointsLtR.bottom);
    }

    @Test
    public void testGradientLine_invalidDegrees() {
        GradientShader shader = new GradientShader(
                new int[] {Color.BLUE, Color.RED}, new float[] {0, 1}, 999, Suppliers.of(LTR));
        assertThat(shader.mAngleRadians).isEqualTo(Math.toRadians(999 % 360));
        assertThat(shader.mAngleRadiansSmall).isEqualTo(Math.toRadians(999 % 90));
    }

    @Test
    public void testGradientLine_invalidStops() {
        GradientShader shader = new GradientShader(
                new int[] {Color.BLUE, Color.RED}, new float[] {999, -1}, 45, Suppliers.of(LTR));
        shader.resize(123, 456);

        // assert no failures
    }

    @Test
    public void testGradientLine_lengthMismatch() {
        assertThatRunnable(()
                                   -> new GradientShader(new int[] {Color.BLUE, Color.RED},
                                           new float[] {1}, 45, Suppliers.of(LTR)))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Mismatch: got 2 colors and 1 stops");
    }

    @Test
    public void testGradientLine_nullLtRSupplier() {
        GradientShader shader =
                new GradientShader(new int[] {Color.BLUE, Color.RED}, new float[] {0, 1}, 45, null);
        shader.resize(123, 456);

        // assert no failures
    }

    private void checkGradientLine(
            RectF result, float startx, float starty, float endx, float endy) {
        String testFailOutput = String.format(
                "expected: %s\nbut was : %s", new RectF(startx, starty, endx, endy), result);
        assertWithMessage(testFailOutput).that(result.left).isWithin(0.01f).of(startx);
        assertWithMessage(testFailOutput).that(result.top).isWithin(0.01f).of(starty);
        assertWithMessage(testFailOutput).that(result.right).isWithin(0.01f).of(endx);
        assertWithMessage(testFailOutput).that(result.bottom).isWithin(0.01f).of(endy);
    }
}
