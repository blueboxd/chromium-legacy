// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ViewHighlighterTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests the utility methods for highlighting of a view.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ViewHighlighterTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mContext.setTheme(R.style.Theme_Chromium_WithWindowAnimation);
    }

    @Test
    @MediumTest
    public void testRepeatedCallsToHighlightWorksCorrectly() {
        View tintedImageButton =
                LayoutInflater.from(mContext).inflate(R.layout.clear_storage, null, false);
        tintedImageButton.setBackground(new ColorDrawable(Color.LTGRAY));
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, true);
        ViewHighlighter.turnOnHighlight(tintedImageButton, true);
        checkHighlightOn(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, false);
        checkHighlightOn(tintedImageButton);
    }

    @Test
    @MediumTest
    public void testViewWithNullBackground() {
        View tintedImageButton =
                LayoutInflater.from(mContext).inflate(R.layout.clear_storage, null, false);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, true);
        checkHighlightOn(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, false);
        checkHighlightOn(tintedImageButton);
    }

    /**
     * Assert that the provided view is highlighted.
     *
     * @param view The view of interest.
     */
    private static void checkHighlightOn(View view) {
        Assert.assertTrue(ViewHighlighterTestUtils.checkHighlightOn(view));
    }

    /**
     * Assert that the provided view is not highlighted.
     *
     * @param view The view of interest.
     */
    private static void checkHighlightOff(View view) {
        Assert.assertTrue(ViewHighlighterTestUtils.checkHighlightOff(view));
    }
}