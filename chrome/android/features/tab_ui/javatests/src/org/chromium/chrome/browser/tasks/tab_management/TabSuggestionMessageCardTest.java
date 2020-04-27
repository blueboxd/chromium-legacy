// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.core.AllOf.allOf;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

/**
 * End-to-end tests for TabSuggestion.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.CLOSE_TAB_SUGGESTIONS+"<Study"})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:baseline_tab_suggestions/true"})
public class TabSuggestionMessageCardTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
    }

    @Test
    @MediumTest
    @FlakyTest(message = "crbug.com/1075650")
    public void closeTabSuggestionReviewedAndAccepted() {
        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        CriteriaHelper.pollUiThread(Criteria.equals(3, currentTabModel::getCount));
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        TabSelectionEditorTestingRobot tabSelectionEditorTestingRobot =
                new TabSelectionEditorTestingRobot();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        tabSelectionEditorTestingRobot.actionRobot.clickToolbarActionButton();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        CriteriaHelper.pollUiThread(Criteria.equals(0, currentTabModel::getCount));
    }

    @Test
    @MediumTest
    public void closeTabSuggestionReviewedAndDismissed() {
        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        CriteriaHelper.pollUiThread(Criteria.equals(3, currentTabModel::getCount));
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        TabSelectionEditorTestingRobot tabSelectionEditorTestingRobot =
                new TabSelectionEditorTestingRobot();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        tabSelectionEditorTestingRobot.actionRobot.clickToolbarNavigationButton();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        CriteriaHelper.pollUiThread(Criteria.equals(3, currentTabModel::getCount));
    }
}
