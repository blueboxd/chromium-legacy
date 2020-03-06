// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import androidx.annotation.ColorInt;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.UrlExpansionObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.ToolbarTestUtils;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.TimeoutException;

/**
 * {@link StatusBarColorController} tests.
 * There are additional status bar color tests in {@link BrandColorTest}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations
        .UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
        @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        public class StatusBarColorControllerTest {
    /**
     * {@link CallbackHelper} for waiting till the URL expansion reaches the passed-in value.
     */
    private static class TestUrlExpansionObserver
            extends CallbackHelper implements UrlExpansionObserver {
        private float mCurrentPercentage = -1.0f;
        private float mWaitingPercentage = -1.0f;

        public void waitForUrlExpansionPercentage(float percentage) throws TimeoutException {
            mWaitingPercentage = percentage;
            if (mWaitingPercentage == mCurrentPercentage) return;

            waitForFirst();
        }

        @Override
        public void onUrlExpansionPercentageChanged(float percentage) {
            mCurrentPercentage = percentage;
            if (mWaitingPercentage == mCurrentPercentage) {
                notifyCalled();
            }
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    private @ColorInt int mScrimColor;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, true);
        mActivityTestRule.startMainActivityOnBlankPage();
        mScrimColor = ApiCompatibilityUtils.getColor(mActivityTestRule.getActivity().getResources(),
                org.chromium.chrome.R.color.black_alpha_65);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, null);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    /**
     * Test that the status bar color is toggled when toggling incognito while in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorToggleIncognitoInOverview() {
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

    /**
     * Test that switching tabs from a page with no brand color to an NTP with a location bar
     * in dark mode updates the status bar color.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSwitchTabPageNoBrandColorToNtp(boolean nightModeEnabled) throws Exception {
        if (!nightModeEnabled) return;

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        Resources resources = activity.getResources();

        final int defaultThemeColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_bg_color_dark_elev_3);
        final int expectedNtpStatusBarColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_bg_color_dark);
        assertNotEquals(defaultThemeColor, expectedNtpStatusBarColor);

        TestUrlExpansionObserver urlExpansionObserver = new TestUrlExpansionObserver();
        TopToolbarCoordinator topToolbarCoordinator =
                (TopToolbarCoordinator) activity.getToolbarManager().getToolbar();
        topToolbarCoordinator.addUrlExpansionObserver(urlExpansionObserver);

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), activity, R.id.new_tab_menu_id);
        assertEquals(2, activity.getCurrentTabModel().getCount());
        NewTabPageTestUtils.waitForNtpLoaded(activity.getActivityTab());
        urlExpansionObserver.waitForUrlExpansionPercentage(0.0f);
        ThemeTestUtils.waitForThemeColor(activity, defaultThemeColor);
        ThemeTestUtils.assertStatusBarColor(activity, expectedNtpStatusBarColor);

        // Return to about:blank by swiping the toolbar right.
        ChromeTabUtils.selectTabWithAction(
                InstrumentationRegistry.getInstrumentation(), activity, () -> {
                    ToolbarTestUtils.performToolbarSideSwipe(activity, ScrollDirection.RIGHT);
                });
        assertFalse(activity.getActivityTab().isNativePage());
        ThemeTestUtils.assertStatusBarColor(activity, defaultThemeColor);
    }

    /**
     * Test that the status indicator color is included in the color calculation correctly.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorWithStatusIndicator() {
        final ChromeActivity activity = mActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                mActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorControllerForTesting();
        final Supplier<Integer> statusBarColor = () -> activity.getWindow().getStatusBarColor();
        final int initialColor = statusBarColor.get();

        // Initially, StatusBarColorController#getStatusBarColorWithoutStatusIndicator should return
        // the same color as the current status bar color.
        Assert.assertEquals(
                "Wrong initial value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set a status indicator color.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.onStatusIndicatorColorChanged(Color.BLUE));

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            Assert.assertEquals("Wrong status bar color for Android L.",
                    ColorUtils.getDarkenedColorForStatusBar(Color.BLUE),
                    statusBarColor.get().intValue());
        } else {
            Assert.assertEquals("Wrong status bar color for Android M+.", Color.BLUE,
                    statusBarColor.get().intValue());
        }

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals("Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.getStatusBarScrimDelegate()
                        .setStatusBarScrimFraction(.5f));

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // If we're already darkening the color for Android L, scrim shouldn't be applied.
            Assert.assertEquals("Wrong status bar color w/ scrim for Android L.",
                    ColorUtils.getDarkenedColorForStatusBar(Color.BLUE),
                    statusBarColor.get().intValue());
        } else {
            // Otherwise, the resulting color should be a scrimmed version of the status bar color.
            Assert.assertEquals("Wrong status bar color w/ scrim for Android M+.",
                    getScrimmedColor(Color.BLUE, .5f), statusBarColor.get().intValue());
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove scrim.
            statusBarColorController.getStatusBarScrimDelegate().setStatusBarScrimFraction(.0f);
            // Set the status indicator color to the default, i.e. transparent.
            statusBarColorController.onStatusIndicatorColorChanged(Color.TRANSPARENT);
        });

        // Now, the status bar color should be back to the initial color.
        Assert.assertEquals(
                "Wrong status bar color after the status indicator color is set to default.",
                initialColor, statusBarColor.get().intValue());
    }

    private int defaultColorFallbackToBlack(int color) {
        return (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) ? Color.BLACK : color;
    }

    private int getScrimmedColor(@ColorInt int color, float fraction) {
        final float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
        final int scrimColorOpaque = mScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(color, scrimColorOpaque, fraction * scrimColorAlpha);
    }
}
