// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarVisibility;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager.Variations;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Test bottom toolbar when start surface is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
public class BottomToolbarWithStartSurfaceTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        FeatureUtilities.setIsBottomToolbarEnabledForTesting(true);
        FeatureUtilities.setStartSurfaceEnabledForTesting(true);
    }

    @After
    public void tearDown() {
        FeatureUtilities.setIsBottomToolbarEnabledForTesting(null);
        FeatureUtilities.setStartSurfaceEnabledForTesting(null);
    }

    private void launchActivity(@Variations String variation) {
        BottomToolbarVariationManager.setVariation(variation);
        mActivityTestRule.startMainActivityFromLauncher();
    }

    @Test
    @MediumTest
    public void testShowAndHideSingleSurface() {
        launchActivity(Variations.HOME_SEARCH_SHARE);
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_HOMEPAGE));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_TABSWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        checkToolbarVisibility(BOTTOM_TOOLBAR, false);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.NOT_SHOWN));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
    }
}
