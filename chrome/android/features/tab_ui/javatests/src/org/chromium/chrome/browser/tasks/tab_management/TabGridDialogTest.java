// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.replaceText;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.intent.Intents.intended;
import static android.support.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasType;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickScrimToExitDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.isPopupTabListCompletelyHidden;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.isPopupTabListCompletelyShowing;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyShowingPopupTabList;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.espresso.intent.Intents;
import android.support.test.espresso.intent.rule.IntentsTestRule;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** End-to-end tests for TabGridDialog component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID})
public class TabGridDialogTest {
    // clang-format on
    private static final String CUSTOMIZED_TITLE1 = "wfh tips";
    private static final String CUSTOMIZED_TITLE2 = "wfh funs";

    private boolean mHasReceivedSourceRect;
    private TabSelectionEditorTestingRobot mSelectionEditorRobot =
            new TabSelectionEditorTestingRobot();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public IntentsTestRule<ChromeActivity> mShareActivityTestRule =
            new IntentsTestRule<>(ChromeActivity.class, false, false);

    @Before
    public void setUp() {
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }

    @After
    public void tearDown() {
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(null);
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().overviewVisible());
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 2, null);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        waitForDialogHidingAnimation(cta);
    }

    @Test
    @MediumTest
    public void testDisableTabGroupsContinuation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is not exposed.
        verifyTabGroupsContinuation(cta, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testEnableTabGroupsContinuation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is exposed.
        verifyTabGroupsContinuation(cta, true);
    }

    @Test
    @MediumTest
    public void testTabGridDialogAnimation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Add 400px top margin to the recyclerView.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        float tabGridCardPadding = cta.getResources().getDimension(R.dimen.tab_list_card_padding);
        int deltaTopMargin = 400;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) recyclerView.getLayoutParams();
        params.topMargin += deltaTopMargin;
        TestThreadUtils.runOnUiThreadBlocking(() -> recyclerView.setLayoutParams(params));
        CriteriaHelper.pollUiThread(() -> !recyclerView.isComputingLayout());

        // Calculate expected values of animation source rect.
        mHasReceivedSourceRect = false;
        View parentView = cta.getCompositorViewHolder();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);
        Rect sourceRect = new Rect();
        recyclerView.getChildAt(0).getGlobalVisibleRect(sourceRect);
        // TODO(yuezhanggg): Figure out why the sourceRect.left is wrong after setting the margin.
        float expectedTop = sourceRect.top - parentRect.top + tabGridCardPadding;
        float expectedWidth = sourceRect.width() - 2 * tabGridCardPadding;
        float expectedHeight = sourceRect.height() - 2 * tabGridCardPadding;

        // Setup the callback to verify the animation source Rect.
        StartSurfaceLayout layout = (StartSurfaceLayout) cta.getLayoutManager().getOverviewLayout();
        TabSwitcher.TabDialogDelegation delegation =
                layout.getStartSurfaceForTesting().getTabDialogDelegate();
        delegation.setSourceRectCallbackForTesting((result -> {
            mHasReceivedSourceRect = true;
            assertEquals(expectedTop, result.top, 0.0);
            assertEquals(expectedHeight, result.height(), 0.0);
            assertEquals(expectedWidth, result.width(), 0.0);
        }));

        TabUiTestHelper.clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> mHasReceivedSourceRect);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
    }

    @Test
    @MediumTest
    public void testUndoClosureInDialog_GTS() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Click close button to close the first tab in group.
        closeFirstTabInDialog(cta);
        verifyShowingDialog(cta, 1, null);

        // Exit dialog, wait for the undo bar showing and undo the closure.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);

        // Verify the undo has happened.
        onView(withId(R.id.tab_title)).check((v, noMatchException) -> {
            TextView textView = (TextView) v;
            assertEquals("2 tabs", textView.getText().toString());
        });
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
    }

    @Test
    @MediumTest
    public void testUndoClosureInDialog_TabStrip() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().overviewVisible());
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);

        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 2, null);

        // Click close button to close the first tab in group.
        closeFirstTabInDialog(cta);
        verifyShowingDialog(cta, 1, null);

        // Exit dialog, wait for the undo bar showing and undo the closure.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);

        // Verify the undo has happened.
        verifyTabStripFaviconCount(cta, 2);
        openDialogFromStripAndVerify(cta, 2, null);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testDialogToolbarMenuShareGroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Click to show the menu and verify it.
        openDialogToolbarMenuAndVerify(cta);

        // Trigger the share sheet by clicking the share button and verify it.
        triggerShareGroupAndVerify(cta);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testSelectionEditorShowHide() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));

        // Back press should close both the dialog and selection editor.
        openSelectionEditorAndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Clicking ScrimView should close both the dialog and selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testSelectionEditorUngroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        final TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                                   .getTabModelFilterProvider()
                                                   .getCurrentTabModelFilter();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals(1, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        openSelectionEditorAndVerify(cta, 3);

        // Select and ungroup the first tab.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("1 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals(2, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Select and ungroup all two tabs in dialog.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(
                1);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("2 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        assertEquals(3, filter.getCount());
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_Dialog() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Swipe to dismiss two tabs in dialog.
        onView((withId(R.id.tab_list_view)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));
        verifyShowingDialog(cta, 1, null);
        onView((withId(R.id.tab_list_view)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        0, getSwipeToDismissAction(false)));
        waitForDialogHidingAnimation(cta);
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testSelectionEditorPosition() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        View parentView = cta.getCompositorViewHolder();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the size and position of TabGridDialog in portrait mode.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        checkPopupPosition(cta, true, true);

        // Verify the size and position of TabSelectionEditor in portrait mode.
        openSelectionEditorAndVerify(cta, 3);
        checkPopupPosition(cta, false, true);

        // Verify the size and position of TabSelectionEditor in landscape mode.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() < parentView.getWidth());
        checkPopupPosition(cta, false, false);

        // Verify the size and position of TabGridDialog in landscape mode.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));
        checkPopupPosition(cta, true, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testTabGroupNaming() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(cta, 2,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 2, 2));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);

        // Modify title in dialog from tab strip.
        clickFirstTabInDialog(cta);
        openDialogFromStripAndVerify(cta, 2, CUSTOMIZED_TITLE1);
        editDialogTitle(cta, CUSTOMIZED_TITLE2);

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        enterTabSwitcher(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE2);
    }

    @Test
    @MediumTest
    public void testDialogInitialShowFromStrip() throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Restart the activity and open the dialog from strip to check the initial setup of dialog.
        TabUiTestHelper.finishActivity(cta);
        mActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
        openDialogFromStripAndVerify(mActivityTestRule.getActivity(), 2, null);
    }

    private void openDialogFromTabSwitcherAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void openDialogFromStripAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        showDialogFromStrip(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void verifyShowingDialog(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        verifyShowingPopupTabList(cta, tabCount);

        // Check contents within dialog.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    Assert.assertTrue(v instanceof EditText);
                    EditText titleText = (EditText) v;
                    String title = customizedTitle == null
                            ? cta.getResources().getQuantityString(
                                    R.plurals.bottom_tab_grid_title_placeholder, tabCount, tabCount)
                            : customizedTitle;
                    Assert.assertEquals(title, titleText.getText().toString());
                    assertFalse(v.isFocused());
                });

        // Check dummy views used for animations are not visible.
        onView(allOf(withParent(withId(R.id.dialog_parent_view)), withId(R.id.dialog_frame)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, e) -> { assertEquals(0f, v.getAlpha(), 0.0); });
        onView(allOf(withParent(withId(R.id.dialog_parent_view)),
                       withId(R.id.dialog_animation_card_view)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, e) -> { assertEquals(0f, v.getAlpha(), 0.0); });
    }

    private boolean isDialogShowing(ChromeTabbedActivity cta) {
        return isPopupTabListCompletelyShowing(cta);
    }

    private boolean isDialogHidden(ChromeTabbedActivity cta) {
        return isPopupTabListCompletelyHidden(cta);
    }

    private void showDialogFromStrip(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().overviewVisible());
        onView(withId(R.id.toolbar_left_button)).perform(click());
    }

    private void verifyTabGroupsContinuation(ChromeTabbedActivity cta, boolean isEnabled) {
        assertEquals(isEnabled, TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        // Verify whether the menu button exists.
        onView(withId(R.id.toolbar_menu_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(isEnabled ? matches(isDisplayed()) : doesNotExist());

        // Try to grab focus of the title text field by clicking on it.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    // Verify if we can grab focus on the editText or not.
                    assertEquals(isEnabled, v.isFocused());
                });
    }

    private void openDialogToolbarMenuAndVerify(ChromeTabbedActivity cta) {
        onView(withId(R.id.toolbar_menu_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        onView(withId(R.id.tab_switcher_action_menu_list))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof ListView);
                    ListView listView = (ListView) v;
                    assertEquals(2, listView.getCount());
                    verifyTabGridDialogToolbarMenuItem(listView, 0,
                            cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group));
                    verifyTabGridDialogToolbarMenuItem(listView, 1,
                            cta.getString(R.string.tab_grid_dialog_toolbar_share_group));
                });
    }

    private void verifyTabGridDialogToolbarMenuItem(ListView listView, int index, String text) {
        View menuItemView = listView.getChildAt(index);
        TextView menuItemText = menuItemView.findViewById(R.id.menu_item_text);
        assertEquals(text, menuItemText.getText());
    }

    private void selectTabGridDialogToolbarMenuItem(ChromeTabbedActivity cta, String buttonText) {
        onView(withText(buttonText))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }

    private void triggerShareGroupAndVerify(ChromeTabbedActivity cta) {
        Intents.init();
        selectTabGridDialogToolbarMenuItem(cta, "Share group");
        // For K and below, we show share dialog; for L and above, we send intent and trigger system
        // share sheet. See ShareSheetMediator.ShareSheetDelegate for more info.
        if (TabUiTestHelper.isKitKatAndBelow()) {
            onView(withId(R.id.action_bar_root))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
            onView(withId(R.id.contentPanel))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } else {
            intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                    hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                            allOf(hasAction(equalTo(Intent.ACTION_SEND)),
                                    hasType("text/plain"))))));
        }
        Intents.release();
    }

    private void waitForDialogHidingAnimation(ChromeTabbedActivity cta) {
        CriteriaHelper.pollInstrumentationThread(() -> isDialogHidden(cta));
    }

    private void waitForDialogHidingAnimationInTabSwitcher(ChromeTabbedActivity cta) {
        waitForDialogHidingAnimation(cta);
        // Animation source card becomes alpha = 0f when dialog is showing and animates back to 1f
        // when dialog hides. Make sure the source card has restored its alpha change.
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
            for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) continue;
                if (viewHolder.itemView.getAlpha() != 1f) return false;
            }
            return true;
        });
    }

    private void openSelectionEditorAndVerify(ChromeTabbedActivity cta, int count) {
        // Open tab selection editor by selecting ungroup item in tab grid dialog menu.
        onView(withId(R.id.toolbar_menu_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        onView(withText(cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        R.string.tab_grid_dialog_selection_mode_remove)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(count);
    }

    private void checkPopupPosition(
            ChromeTabbedActivity cta, boolean isDialog, boolean isPortrait) {
        // If isDialog is true, we are checking the position of TabGridDialog; otherwise we are
        // checking the position of TabSelectionEditor.
        int contentViewId = isDialog ? R.id.dialog_container_view : R.id.selectable_list;
        int smallMargin =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        int largeMargin = (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        int topMargin = isPortrait ? largeMargin : smallMargin;
        int sideMargin = isPortrait ? smallMargin : largeMargin;
        View parentView = cta.getCompositorViewHolder();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);

        onView(withId(contentViewId))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, e) -> {
                    int[] location = new int[2];
                    v.getLocationOnScreen(location);
                    // Check the position.
                    assertEquals(sideMargin, location[0]);
                    assertEquals(topMargin + parentRect.top, location[1]);
                    // Check the size.
                    assertEquals(parentView.getHeight() - 2 * topMargin, v.getHeight());
                    assertEquals(parentView.getWidth() - 2 * sideMargin, v.getWidth());
                });
    }

    private void editDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click(), replaceText(title));
    }

    private void verifyFirstCardTitle(String title) {
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    RecyclerView recyclerView = (RecyclerView) v;
                    TextView firstCardTitleTextView =
                            recyclerView.findViewHolderForAdapterPosition(0).itemView.findViewById(
                                    R.id.tab_title);
                    assertEquals(title, firstCardTitleTextView.getText().toString());
                });
    }
}
