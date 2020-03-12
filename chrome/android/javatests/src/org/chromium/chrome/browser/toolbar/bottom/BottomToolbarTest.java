// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR_HOME;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR_NEW_TAB;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR_SEARCH;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR_SHARE;
import static org.chromium.chrome.test.util.ToolbarTestUtils.BOTTOM_TOOLBAR_TAB_SWITCHER;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarButtonVisibility;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarVisibility;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager.Variations;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the bottom toolbar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.CHROME_DUET})
public class BottomToolbarTest {
    //clang-format on
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);


    @Test
    @MediumTest
    public void testBottomToolbar_Home_Search_Tab_Switcher() throws ExecutionException {
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_TAB_SWITCHER);
        mActivityTestRule.startMainActivityOnBlankPage();

        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_HOME, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SEARCH, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_TAB_SWITCHER, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_NEW_TAB, false);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SHARE, false);

        ViewGroup bottomToolbar = mActivityTestRule.getActivity().findViewById(BOTTOM_TOOLBAR);
        View tabSwitcherButton = bottomToolbar.findViewById(BOTTOM_TOOLBAR_TAB_SWITCHER);

        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getOverviewModeBehavior(), true, false);
        TestThreadUtils.runOnUiThreadBlocking(() -> tabSwitcherButton.callOnClick());
        overviewModeWatcher.waitForBehavior();

        Assert.assertTrue("Tab switcher should be visible.",
                mActivityTestRule.getActivity().getOverviewModeBehavior().overviewVisible());
    }

    @Test
    @MediumTest
    public void testBottomToolbar_New_Tab_Search_Share() throws ExecutionException {
        BottomToolbarVariationManager.setVariation(Variations.NEW_TAB_SEARCH_SHARE);
        mActivityTestRule.startMainActivityOnBlankPage();

        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_NEW_TAB, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SEARCH, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SHARE, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_HOME, false);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_TAB_SWITCHER, false);

        int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ViewGroup bottomToolbar = mActivityTestRule.getActivity().findViewById(BOTTOM_TOOLBAR);
        View newTabButton = bottomToolbar.findViewById(BOTTOM_TOOLBAR_NEW_TAB);
        TestThreadUtils.runOnUiThreadBlocking(() -> newTabButton.callOnClick());

        Assert.assertEquals(
                tabCount + 1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    @Test
    @MediumTest
    public void testBottomToolbar_Home_Search_Share() {
        BottomToolbarVariationManager.setVariation(Variations.HOME_SEARCH_SHARE);
        mActivityTestRule.startMainActivityOnBlankPage();

        checkToolbarVisibility(BOTTOM_TOOLBAR, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_HOME, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SEARCH, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_SHARE, true);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_NEW_TAB, false);
        checkToolbarButtonVisibility(BOTTOM_TOOLBAR, BOTTOM_TOOLBAR_TAB_SWITCHER, false);
    }
}
