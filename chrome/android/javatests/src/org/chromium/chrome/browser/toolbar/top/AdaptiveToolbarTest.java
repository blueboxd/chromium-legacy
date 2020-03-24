// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_MENU;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TOP_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TOP_TOOLBAR_HOME;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TOP_TOOLBAR_MENU;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TOP_TOOLBAR_TAB_SWITCHER;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarButtonVisibility;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarVisibility;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager.Variations;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

/** End-to-end tests for adaptive toolbar. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
public class AdaptiveToolbarTest {
    // Params to turn off new tab variation in GTS.
    private static final String NO_NEW_TAB_VARIATION_PARAMS = "force-fieldtrial-params=" +
            "Study.Group:tab_grid_layout_android_new_tab/false";
    // Params to turn on new tab variation in GTS.
    private static final String NEW_TAB_VARIATION_PARAMS = "force-fieldtrial-params=" +
            "Study.Group:tab_grid_layout_android_new_tab/NewTabVariation";
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @After
    public void tearDown() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, null);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, null);
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    private void setupFlagsAndLaunchActivity(
            boolean isBottomToolbarEnabled, boolean isGridTabSwitcherEnabled) {
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, isGridTabSwitcherEnabled);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, isBottomToolbarEnabled);
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS_WithBottomToolbar_Home_Search_Tab_Switcher() {
        // clang-format on
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_TAB_SWITCHER);
        setupFlagsAndLaunchActivity(true, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, false);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_HOME, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        checkToolbarVisibility(BOTTOM_TOOLBAR, false);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS_WithBottomToolbar_Home_Search_Tab_Switcher_IncognitoDisabled() {
        // clang-format on
        IncognitoUtils.setEnabledForTesting(false);
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_TAB_SWITCHER);
        setupFlagsAndLaunchActivity(true, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, false);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_HOME, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        // Show bottom toolbar when incognito is disabled.
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);

        // Reset incognito.
        IncognitoUtils.setEnabledForTesting(null);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS_WithBottomToolbar_Home_Search_Share() {
        // clang-format on
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_SHARE);
        setupFlagsAndLaunchActivity(true, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_HOME, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Failed on android-arm-stable-tests. crbug.com/1062634")
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS_WithBottomToolbar_New_Tab_Search_Share() {
        // clang-format on
        BottomToolbarVariationManager.setVariation(Variations.NEW_TAB_SEARCH_SHARE);
        setupFlagsAndLaunchActivity(true, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Menu button should be visible and new tab button should be invisible
        // in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);
        // Bottom toolbar should be visible.
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        // Both menu and new tab button should be visible in top tab switcher toolbar.
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS_WithoutBottomToolbar() throws InterruptedException {
        // clang-format on
        setupFlagsAndLaunchActivity(false, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
    }

    @Test
    @MediumTest
    public void testTopToolbar_WithoutGTS_WithBottomToolbar_Home_Search_Tab_Switcher() {
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_TAB_SWITCHER);
        setupFlagsAndLaunchActivity(true, false);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertFalse(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, false);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_HOME, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    public void testTopToolbar_WithoutGTS_WithBottomToolbar_Home_Search_Share() {
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_SHARE);
        setupFlagsAndLaunchActivity(true, false);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertFalse(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_HOME, false);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    public void testTopToolbar_WithoutGTS_WithBottomToolbar_New_Tab_Search_Share() {
        BottomToolbarVariationManager.setVariation(Variations.NEW_TAB_SEARCH_SHARE);
        setupFlagsAndLaunchActivity(true, false);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertFalse(layout instanceof StartSurfaceLayout);

        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TOP_TOOLBAR, TOP_TOOLBAR_TAB_SWITCHER, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        enterTabSwitcher(cta);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_TAB_SWITCHER_BUTTON, true);
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_NewTabVariation() {
        // clang-format on
        setupFlagsAndLaunchActivity(false, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_NewTabVariation_IncognitoDisabled() {
        // clang-format on
        IncognitoUtils.setEnabledForTesting(false);
        setupFlagsAndLaunchActivity(false, true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = cta.getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, false);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, false);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
    }
}
