// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.isPopupTabListCompletelyHidden;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.isPopupTabListCompletelyShowing;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyShowingPopupTabList;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager.Variations;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.UiSwitches;
import org.chromium.ui.test.util.UiRestriction;

import java.util.List;

/** End-to-end tests for TabGroupPopupUi component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
// TODO(crbug.com/1058231): ENABLE_SCREENSHOT_UI_MODE is to disable IPHs for TabGroups.
//  We should make this test more robust so that it's agnostic of IPHs.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        UiSwitches.ENABLE_SCREENSHOT_UI_MODE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.CHROME_DUET,
        ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID})
public class TabGroupPopupUiTest {
    // clang-format on

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testOnAnchorViewChanged_HOME_SEARCH_TAB_SWITCHER() {
        launchActivity(Variations.HOME_SEARCH_TAB_SWITCHER);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Tab strip should show automatically when entering tab group.
        createTabGroupAndEnterTabPage(cta, 2, null);
        CriteriaHelper.pollInstrumentationThread(() -> !isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        // In portrait mode, tab strip should anchor on bottom toolbar; in landscape mode, bottom
        // toolbar hides and tab strip should anchor on top toolbar.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollInstrumentationThread(() -> !isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);
    }

    @Test
    @MediumTest
    public void testOnAnchorViewChanged_HOME_SEARCH_SHARE() {
        launchActivity(Variations.HOME_SEARCH_SHARE);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Tab strip should show automatically when entering tab group.
        createTabGroupAndEnterTabPage(cta, 2, null);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        // In portrait mode, tab strip should anchor on bottom toolbar; in landscape mode, bottom
        // toolbar hides and tab strip should anchor on top toolbar.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);
    }

    @Test
    @MediumTest
    public void testOnAnchorViewChanged_NEW_TAB_SEARCH_SHARE() {
        launchActivity(Variations.NEW_TAB_SEARCH_SHARE);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Tab strip should show automatically when entering tab group.
        createTabGroupAndEnterTabPage(cta, 2, null);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        // In portrait mode, tab strip should anchor on bottom toolbar; in landscape mode, bottom
        // toolbar hides and tab strip should anchor on top toolbar.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollInstrumentationThread(() -> isAnchoredOnTopToolbar(cta));
        verifyShowingTabStrip(cta, 2);
    }

    @Test
    @MediumTest
    public void testTabStripShowHide() {
        launchActivity();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Try to trigger tab strip in a single tab page.
        triggerTabStripAndVerify(cta, 0);

        // Tab strip should show automatically when entering tab group.
        createTabGroupAndEnterTabPage(cta, 2, null);
        verifyShowingTabStrip(cta, 2);

        // Dismiss tab strip by clicking the left button.
        onView(withId(R.id.toolbar_left_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripHidden(cta));

        // Re-show the tab strip.
        triggerTabStripAndVerify(cta, 2);

        // Tab strip should not show when overview mode is visible.
        enterTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripHidden(cta));

        // Re-verify that tab strip never shows in single tab.
        clickFirstCardFromTabSwitcher(cta);
        closeFirstTabInDialog(cta);
        clickFirstTabInDialog(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripHidden(cta));
        triggerTabStripAndVerify(cta, 0);
    }

    @Test
    @MediumTest
    public void testTabStripUpdate() {
        launchActivity();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Tab strip should show automatically when entering tab group.
        createTabGroupAndEnterTabPage(cta, 2, null);
        verifyShowingTabStrip(cta, 2);

        // Adding tabs should trigger tab strip update.
        onView(withId(R.id.toolbar_right_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        verifyShowingTabStrip(cta, 3);

        onView(withId(R.id.toolbar_right_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        verifyShowingTabStrip(cta, 4);

        // Click on the current tab in strip to close, and the closure should trigger tab strip
        // update.
        onView(withId(R.id.tab_list_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        getCurrentTabIndexInGroup(cta), click()));
        verifyShowingTabStrip(cta, 3);

        onView(withId(R.id.tab_list_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        getCurrentTabIndexInGroup(cta), click()));
        verifyShowingTabStrip(cta, 2);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION})
    public void testTabStripChangeWithScrolling() {
        launchActivity();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        FullscreenManagerTestUtils.disableBrowserOverrides();

        // Tab strip should show automatically when entering tab group.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String url = testServer.getURL("/chrome/test/data/android/navigate/three.html");
        createTabGroupAndEnterTabPage(cta, 2, url);
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripShowing(cta));

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);

        onView(withId(R.id.tab_group_ui_toolbar_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, e) -> {
                    View stripContainerView = (View) v.getParent();
                    assertTrue(stripContainerView instanceof FrameLayout);
                    assertEquals(0f, stripContainerView.getAlpha(), 0);
                });

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, true);

        onView(withId(R.id.tab_group_ui_toolbar_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, e) -> {
                    View stripContainerView = (View) v.getParent();
                    assertTrue(stripContainerView instanceof FrameLayout);
                    assertEquals(1f, stripContainerView.getAlpha(), 0);
                });
    }

    private void createTabGroupAndEnterTabPage(ChromeTabbedActivity cta, int tabCount, String url) {
        if (url == null) {
            createTabs(cta, false, tabCount);
        } else {
            prepareTabsWithThumbnail(mActivityTestRule, tabCount, 0, url);
        }
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, tabCount);

        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);

        assertFalse(cta.getLayoutManager().overviewVisible());
    }

    /**
     * Try to show tab strip through long press the tab switcher button and verify the tab count in
     * tab strip. If {@code count} is 0, verify that the strip is not showing.
     * @param cta   The current running activity.
     * @param count The count of tabs in tab strip. Could be 0.
     */
    private void triggerTabStripAndVerify(ChromeTabbedActivity cta, int count) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.findViewById(R.id.tab_switcher_button).performLongClick(); });
        if (count == 0) {
            CriteriaHelper.pollInstrumentationThread(() -> isTabStripHidden(cta));
            return;
        }
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripShowing(cta));
        verifyShowingTabStrip(cta, count);
    }

    private boolean isAnchoredOnTopToolbar(ChromeTabbedActivity cta) {
        boolean isTop = true;
        try {
            onView(withId(R.id.tab_list_view))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check((v, e) -> {
                        int[] tabStripPosition = new int[2];
                        int[] topToolbarPosition = new int[2];

                        v.getLocationOnScreen(tabStripPosition);
                        View topToolbarView = cta.findViewById(R.id.toolbar);
                        topToolbarView.getLocationOnScreen(topToolbarPosition);

                        // If strip is anchored on top toolbar, it should locate close to the top
                        // toolbar.
                        int anchorDistance = topToolbarView.getHeight();
                        assertTrue(tabStripPosition[1] - topToolbarPosition[1] < anchorDistance);
                    });
        } catch (AssertionError e) {
            isTop = false;
        } catch (Exception e) {
            assert false : "error when inspecting pop up tab list.";
        }
        return isTop;
    }

    private boolean isTabStripShowing(ChromeTabbedActivity cta) {
        return isPopupTabListCompletelyShowing(cta);
    }

    private boolean isTabStripHidden(ChromeTabbedActivity cta) {
        return isPopupTabListCompletelyHidden(cta);
    }

    private void verifyShowingTabStrip(ChromeTabbedActivity cta, int tabCount) {
        verifyShowingPopupTabList(cta, tabCount);
    }

    private int getCurrentTabIndexInGroup(ChromeTabbedActivity cta) {
        Tab currentTab = cta.getTabModelSelector().getCurrentTab();
        List<Tab> tabGroup = cta.getTabModelSelector()
                                     .getTabModelFilterProvider()
                                     .getCurrentTabModelFilter()
                                     .getRelatedTabList(currentTab.getId());
        return tabGroup.indexOf(currentTab);
    }

    private void launchActivity() {
        launchActivity(Variations.HOME_SEARCH_TAB_SWITCHER);
    }

    private void launchActivity(@Variations String variation) {
        BottomToolbarVariationManager.setVariation(variation);
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }
}
