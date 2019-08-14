// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for SearchEngineLogoUtils.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchEngineLogoUtilsTest {
    @Test
    public void getMostCommonEdgeColor_allOneColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImage(100, 100, color);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    @Test
    public void getMostCommonEdgeColor_outerInnerColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImageWithDifferentInnerColor(100, 100, color, Color.RED);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    @Test
    public void getMostCommonEdgeColor_slightlyLargerColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImageWithSlighlyLargerEdgeCoverage(100, 100, color, Color.RED);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    private static Bitmap createSolidImage(int width, int height, int color) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = 0; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, color);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithDifferentInnerColor(
            int width, int height, int outerColor, int innerColor) {
        Bitmap bitmap = createSolidImage(width, height, outerColor);
        for (int x = 1; x < bitmap.getWidth() - 1; x++) {
            for (int y = 1; y < bitmap.getHeight() - 1; y++) {
                bitmap.setPixel(x, y, innerColor);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithSlighlyLargerEdgeCoverage(
            int width, int height, int largerColor, int smallerColor) {
        Bitmap bitmap = createSolidImage(width, height, largerColor);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = bitmap.getHeight() + 1; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, smallerColor);
            }
        }
        return bitmap;
    }
}
