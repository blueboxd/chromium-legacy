// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickScrimToExitDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingRootException;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/** End-to-end tests for TabGridIph component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
@Features.DisableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)
public class TabGridIphTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Before
    public void setUp() {
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
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testShowAndHideIphDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        // Check the IPH message card is showing and open the IPH dialog.
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Exit by clicking the "OK" button.
        exitIphDialogByClickingButton(cta);
        verifyIphDialogHiding(cta);

        // Check the IPH message card is showing and open the IPH dialog.
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Exit by clicking the scrim view.
        clickScrimToExitDialog(cta);
        verifyIphDialogHiding(cta);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDismissIphItem() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Restart chrome to verify that IPH message card is still there.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Remove the message card and dismiss the feature by clicking close button.
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Restart chrome to verify that IPH message card no longer shows.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Portrait() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.tab_grid_message_item), "iph_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Landscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.tab_grid_message_item), "iph_landscape");
    }

    @Test
    @MediumTest
    public void testIphItemChangeWithLastTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in tab switcher and the IPH item should not be showing.
        closeFirstTabInTabSwitcher();
        CriteriaHelper.pollUiThread(() -> !TabSwitcherCoordinator.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Undo the closure of the last tab and the IPH item should reshow.
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in the tab switcher.
        closeFirstTabInTabSwitcher();
        CriteriaHelper.pollUiThread(() -> !TabSwitcherCoordinator.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Add the first tab to an empty tab switcher and the IPH item should show.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_IPH() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        RecyclerView.ViewHolder viewHolder = ((RecyclerView) cta.findViewById(R.id.tab_list_view))
                                                     .findViewHolderForAdapterPosition(1);
        assertEquals(TabProperties.UiType.MESSAGE, viewHolder.getItemViewType());

        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    private void verifyIphDialogShowing(ChromeTabbedActivity cta) {
        onView(withId(R.id.iph_dialog))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    String title = cta.getString(R.string.iph_drag_and_drop_title);
                    assertEquals(title, ((TextView) v.findViewById(R.id.title)).getText());

                    String description = cta.getString(R.string.iph_drag_and_drop_content);
                    assertEquals(
                            description, ((TextView) v.findViewById(R.id.description)).getText());

                    String closeButtonText = cta.getString(R.string.ok);
                    assertEquals(closeButtonText,
                            ((TextView) v.findViewById(R.id.close_button)).getText());
                });
    }

    private void verifyIphDialogHiding(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.iph_dialog))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting iph dialog.";
        }
        assertFalse(isShowing);
    }

    private void exitIphDialogByClickingButton(ChromeTabbedActivity cta) {
        onView(withId(R.id.close_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }
}
