// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.os.Build.VERSION_CODES.M;
import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.longClick;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabsTest.SimulateClickOnMainThread;
import org.chromium.chrome.browser.TabsTest.SimulateTabSwipeOnMainThread;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;

/** End-to-end tests for conditional tab strip component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({CONDITIONAL_TAB_STRIP_ANDROID})
@Features.DisableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID, START_SURFACE_ANDROID})
public class ConditionalTabStripTest {
    // clang-format on
    private static final int TEST_SESSION_MS = 600000;
    private static final int SWIPE_TO_RIGHT_DIRECTION = 1;
    private static final int SWIPE_TO_LEFT_DIRECTION = -1;
    private SimulateTabSwipeOnMainThread mSwipeToNormal;
    private SimulateTabSwipeOnMainThread mSwipeToIncognito;
    private float mPxToDp = 1f;
    private float mTabsViewHeightDp;
    private float mTabsViewWidthDp;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        // For this test suite, the session time is set to be 0 by default so that we can start a
        // new session by restarting Chrome.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(0);

        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);

        float dpToPx = InstrumentationRegistry.getInstrumentation()
                               .getContext()
                               .getResources()
                               .getDisplayMetrics()
                               .density;
        mPxToDp = 1.0f / dpToPx;
        View tabsView = mActivityTestRule.getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        mSwipeToIncognito =
                new SimulateTabSwipeOnMainThread(cta.getLayoutManager(), mTabsViewWidthDp - 20,
                        mTabsViewHeightDp / 2, SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp, 0);
        mSwipeToNormal = new SimulateTabSwipeOnMainThread(cta.getLayoutManager(), 20,
                mTabsViewHeightDp / 2, SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp, 0);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = M, message = "crbug.com/1081832")
    public void testStrip_updateWithAddition() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        // Unintentional tab creation will not trigger tab strip.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_EXTERNAL_APP);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 2, 0);
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_LINK);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 3, 0);

        // Intentional tab creation from toolbar menu will trigger tab strip.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        verifyShowingStrip(cta, false, 4);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 4, 0);

        // Intentional tab creation from long-press context menu will trigger tab strip.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        verifyShowingStrip(cta, false, 5);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 5, 0);

        // Intentional tab creation from long-press tab switcher action menu will trigger tab strip.
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_new_tab)).perform(click());
        verifyShowingStrip(cta, false, 6);

        // When tab strip is already showing, both intentional and unintentional tab creation should
        // trigger tab strip update.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_EXTERNAL_APP);
        verifyShowingStrip(cta, false, 7);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        verifyShowingStrip(cta, false, 8);
    }

    @Test
    @MediumTest
    public void testStrip_updateWithClosure() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);

        // Close one tab in tab switcher, check the update is reflected in tab strip.
        enterTabSwitcher(cta);
        closeStackTabAtIndex(cta, 2, false);
        clickOnStackTabAtIndex(1, false);
        verifyShowingStrip(cta, false, 2);

        // Close another tab in tab switcher. Since there is only tab left in current tab model,
        // the strip should be hidden.
        enterTabSwitcher(cta);
        closeStackTabAtIndex(cta, 1, false);
        clickOnStackTabAtIndex(0, false);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_updateWithSelection() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 4, 0);

        // Tab selection through tab switcher should trigger tab strip, and tab selection will be
        // reflected in tab strip.
        for (int i = 0; i < 4; i++) {
            enterTabSwitcher(cta);
            verifyHidingStrip();
            clickOnStackTabAtIndex(i, false);
            verifyShowingStrip(cta, false, 4);
            verifyStripSelectedPosition(cta, i);
        }
    }

    @Test
    @MediumTest
    public void testStrip_updateWithTabModelSwitch() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);
        createTabs(cta, true, 2);
        verifyShowingStrip(cta, true, 2);
        verifyTabModelTabCount(cta, 3, 2);
        assertTrue(cta.getTabModelSelector().isIncognitoSelected());

        // Switch tab model through tab switcher.
        enterTabSwitcher(cta);
        verifyHidingStrip();
        switchTabModel(cta, false);
        verifyHidingStrip();
        clickOnStackTabAtIndex(0, false);
        verifyShowingStrip(cta, false, 3);

        // Switch tab model through creating new tabs.
        createTabs(cta, true, 2);
        verifyTabModelTabCount(cta, 3, 4);
        verifyShowingStrip(cta, true, 4);
        createTabs(cta, false, 3);
        verifyTabModelTabCount(cta, 5, 4);
        verifyShowingStrip(cta, false, 5);
    }

    @Test
    @MediumTest
    public void testStrip_createTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        // Test creating normal tabs by clicking the plus button in tab strip.
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);
        int normalTabCount = 3;
        for (int i = 0; i < 3; i++) {
            clickPlusButtonOnStrip();
            verifyTabModelTabCount(cta, ++normalTabCount, 0);
            verifyShowingStrip(cta, false, normalTabCount);
        }

        // Switch to incognito tab model.
        createTabs(cta, true, 1);
        // Strip should not be showing when there is only one tab in current model.
        verifyHidingStrip();
        createTabs(cta, true, 1);
        verifyShowingStrip(cta, true, 2);

        // Test creating incognito tabs by clicking the plus button in tab strip.
        int incognitoTabCount = 2;
        for (int i = 0; i < 3; i++) {
            clickPlusButtonOnStrip();
            verifyTabModelTabCount(cta, normalTabCount, ++incognitoTabCount);
            verifyShowingStrip(cta, true, incognitoTabCount);
        }
    }

    @Test
    @MediumTest
    public void testStrip_switchTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 4);
        verifyShowingStrip(cta, false, 4);
        verifyStripSelectedPosition(cta, 3);

        // Switching between tabs by clicking on favicon.
        for (int i = 0; i < 4; i++) {
            clickNthItemInStrip(i);
            verifyStripSelectedPosition(cta, i);
        }
    }

    @Test
    @MediumTest
    public void testStrip_closeTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyShowingStrip(cta, false, 2);
        verifyStripSelectedPosition(cta, 1);

        // Click the selected item to close the second-to-last tab, and the strip should be hidden
        // after closure.
        clickNthItemInStrip(1);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 1, 0);

        // Click undo to bring back the second-to-last tab, and should bring back the tab strip as
        // well.
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyShowingStrip(cta, false, 2);
        verifyTabModelTabCount(cta, 2, 0);

        // Disable undo snackbar and test continuous closures.
        cta.getSnackbarManager().disableForTesting();
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 4);
        int tabCount = 4;
        for (int i = 3; i > 0; i--) {
            verifyStripSelectedPosition(cta, i);
            clickNthItemInStrip(i);
            verifyTabModelTabCount(cta, --tabCount, 0);
            if (i == 1) {
                // Tab strip will be hidden when there is only one tab in current model.
                verifyHidingStrip();
            } else {
                verifyShowingStrip(cta, false, tabCount);
            }
        }
    }

    @Test
    @MediumTest
    public void testStrip_dismiss() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        verifyHidingStrip();

        triggerStripAndDismiss(cta);

        // Tab strip should keep hidden throughout this session.
        enterTabSwitcher(cta);
        clickOnStackTabAtIndex(0, false);
        verifyHidingStrip();
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyHidingStrip();
        createTabs(cta, true, 2);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_disabled_expired() throws Exception {
        triggerStripAndDismiss(mActivityTestRule.getActivity());

        ChromeTabbedActivity cta = restartChrome();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyShowingStrip(cta, false, cta.getCurrentTabModel().getCount());
    }

    @Test
    @MediumTest
    public void testStrip_disabled_notExpired() throws Exception {
        triggerStripAndDismiss(mActivityTestRule.getActivity());

        // Update the session time so that the disabled state is not expired for next restart.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        ChromeTabbedActivity cta = restartChrome();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_enabled_expired() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        restartChrome();
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_enabled_notExpired() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Update the session time so that the disabled state is not expired for next restart.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        cta = restartChrome();
        verifyShowingStrip(cta, false, 4);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1081697")
    public void testStrip_UndoDismiss() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Dismiss the strip, and then click on the undo snack bar to bring the strip back.
        clickDismissButtonInStrip();
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyShowingStrip(cta, false, 4);

        // Update the session time so that the enabled state is not expired for next restart. Verify
        // that the undo correctly updated the feature status to enabled.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        cta = restartChrome();
        verifyShowingStrip(cta, false, 4);
    }

    private ChromeTabbedActivity restartChrome() throws Exception {
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        // Wait for bottom controls to stabilize.
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getFullscreenManager()
                                                       .getBottomControlOffset()
                        == 0);
        return mActivityTestRule.getActivity();
    }

    private void createBlankPageWithLaunchType(ChromeTabbedActivity cta, boolean isIncognito,
            @TabLaunchType int type) throws ExecutionException {
        TabCreatorManager.TabCreator tabCreator = cta.getTabCreator(isIncognito);
        LoadUrlParams loadUrlParams = new LoadUrlParams(UrlConstants.CHROME_BLANK_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabCreator.createNewTab(loadUrlParams, type, null));
    }

    private void verifyHidingStrip() {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(not(isDisplayed())));
    }

    private void verifyShowingStrip(ChromeTabbedActivity cta, boolean isIncognito, int tabCount) {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()));
        verifyTabStripFaviconCount(cta, tabCount);
        TabModel tabModel = cta.getTabModelSelector().getModel(isIncognito);
        assertEquals(isIncognito, cta.getTabModelSelector().isIncognitoSelected());
        assertEquals(tabCount, tabModel.getCount());
    }

    private void switchTabModel(ChromeTabbedActivity cta, boolean isIncognito) {
        assertTrue(cta.getOverviewModeBehavior().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(isIncognito ? mSwipeToIncognito : mSwipeToNormal);
        // Wait until the target stack is visible.
        Stack stack = getStack(cta.getLayoutManager(), isIncognito);
        LayoutTab layoutTab = stack.getTabs()[0].getLayoutTab();
        CriteriaHelper.pollUiThread(
                () -> layoutTab.getX() > 0 && layoutTab.getX() < mTabsViewWidthDp);
    }

    private void clickOnStackTabAtIndex(int index, boolean isIncognito) {
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        float[] coordinates = getStackTabClickTarget(index, isIncognito);
        float clickX = coordinates[0];
        float clickY = coordinates[1];

        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(layoutManager, false, true);

        TestThreadUtils.runOnUiThreadBlocking(
                new SimulateClickOnMainThread(layoutManager, (int) clickX, (int) clickY));
        overviewModeWatcher.waitForBehavior();
    }

    private void closeStackTabAtIndex(ChromeTabbedActivity cta, int index, boolean isIncognito) {
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        StackLayout layout = (StackLayout) layoutManager.getOverviewLayout();
        Stack stack = layout.getTabStackAtIndex(
                isIncognito ? StackLayout.INCOGNITO_STACK_INDEX : StackLayout.NORMAL_STACK_INDEX);
        assertTrue(
                "try to close tab at invalid index", index < stack.getTabs().length && index >= 0);
        LayoutTab layoutTab = stack.getTabs()[index].getLayoutTab();
        float x = layoutTab.getCloseBounds().centerX();
        float y = layoutTab.getCloseBounds().centerY();
        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(), cta,
                ()
                        -> TestThreadUtils.runOnUiThreadBlocking(
                                new SimulateClickOnMainThread(layoutManager, x, y)));
    }

    private void verifyStripSelectedPosition(ChromeTabbedActivity cta, int index) {
        assertEquals(cta.getCurrentTabModel().index(), index);
        // Since View.getForeground() is not supported in 23-, there is not good way for us to
        // check the selected item from the perspective of Android View. Therefore, skip this check
        // for API below 23.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            return;
        }
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()))
                .check((v, e) -> {
                    RecyclerView recyclerView = (RecyclerView) v;
                    RecyclerView.Adapter adapter = recyclerView.getAdapter();
                    for (int i = 0; i < adapter.getItemCount(); i++) {
                        View itemView = recyclerView.findViewHolderForAdapterPosition(i).itemView;
                        if (itemView.getForeground() != null) {
                            assertEquals(index, i);
                        }
                    }
                });
    }

    private void clickPlusButtonOnStrip() {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.toolbar_right_button)))
                .perform(click());
    }

    private void clickNthItemInStrip(int index) {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()))
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, click()));
    }

    private void clickDismissButtonInStrip() {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.toolbar_left_button)))
                .perform(click());
    }

    private void triggerStripAndDismiss(ChromeTabbedActivity cta) {
        int normalTabCount = cta.getTabModelSelector().getModel(false).getCount();
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, normalTabCount + 2);

        // Click the left button should dismiss the tab strip.
        clickDismissButtonInStrip();
        verifyHidingStrip();
    }

    // Utility methods copied from TabsTest.java.
    // TODO(yuezhanggg): Pull out these methods into a separate utility class and share them with
    // TabsTest.
    private float[] getStackTabClickTarget(final int tabIndexToSelect, final boolean isIncognito) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] target = new float[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            StackTab[] tabs = stack.getTabs();
            // The position of the click is expressed from the top left corner of the content.
            // The aim is to find an offset that is inside the content but not on the close
            // button.  For this, we calculate the center of the visible tab area.
            LayoutTab layoutTab = tabs[tabIndexToSelect].getLayoutTab();
            LayoutTab nextLayoutTab = (tabIndexToSelect + 1) < tabs.length
                    ? tabs[tabIndexToSelect + 1].getLayoutTab()
                    : null;

            float tabOffsetX = layoutTab.getX();
            float tabOffsetY = layoutTab.getY();
            float tabRightX = tabOffsetX + layoutTab.getScaledContentWidth();
            float tabBottomY = nextLayoutTab != null
                    ? nextLayoutTab.getY()
                    : tabOffsetY + layoutTab.getScaledContentHeight();
            tabRightX = Math.min(tabRightX, mTabsViewWidthDp);
            tabBottomY = Math.min(tabBottomY, mTabsViewHeightDp);

            target[0] = (tabOffsetX + tabRightX) / 2.0f;
            target[1] = (tabOffsetY + tabBottomY) / 2.0f;
        });
        return target;
    }

    private Stack getStack(final LayoutManagerChrome layoutManager, boolean isIncognito) {
        LayoutManagerChromePhone layoutManagerPhone = (LayoutManagerChromePhone) layoutManager;
        StackLayout layout = (StackLayout) layoutManagerPhone.getOverviewLayout();
        return (layout).getTabStackAtIndex(
                isIncognito ? StackLayout.INCOGNITO_STACK_INDEX : StackLayout.NORMAL_STACK_INDEX);
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = mActivityTestRule.getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return mActivityTestRule.getActivity().getLayoutManager();
    }
}
