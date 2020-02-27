// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * {@link StatusBarColorController} tests.
 * There are additional status bar color tests in {@link BrandColorTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class StatusBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, true);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, null);
    }

    /**
     * Test that the status bar color is toggled when toggling incognito while in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorToggleIncongitoInOverview() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        Resources resources = activity.getResources();
        final int expectedOverviewStandardColor = defaultColorFallbackToBlack(
                ChromeColors.getPrimaryBackgroundColor(resources, false));
        final int expectedOverviewIncognitoColor = defaultColorFallbackToBlack(
                ChromeColors.getPrimaryBackgroundColor(resources, true));

        mActivityTestRule.loadUrlInNewTab(
                "about:blank", true /* incognito */, TabLaunchType.FROM_CHROME_UI);
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tabModelSelector.selectModel(true /* incongito */); });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getLayoutManager().showOverview(false /* animate */); });

        ThemeTestUtils.assertStatusBarColor(activity, expectedOverviewIncognitoColor);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tabModelSelector.selectModel(false /* incongito */); });
        ThemeTestUtils.assertStatusBarColor(activity, expectedOverviewStandardColor);
    }

    /**
     * Test that the default color (and not the active tab's brand color) is used in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredInOverview() throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        Resources resources = activity.getResources();
        final int expectedDefaultStandardColor =
                defaultColorFallbackToBlack(ChromeColors.getDefaultThemeColor(resources, false));

        String pageWithBrandColorUrl = mActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        mActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        ThemeTestUtils.assertStatusBarColor(activity, Color.RED);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getLayoutManager().showOverview(false /* animate */); });
        ThemeTestUtils.assertStatusBarColor(activity, expectedDefaultStandardColor);
    }

    private int defaultColorFallbackToBlack(int color) {
        return (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) ? Color.BLACK : color;
    }
}
