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
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import android.support.test.espresso.NoMatchingRootException;
import android.support.test.filters.MediumTest;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

/** End-to-end tests for TabGridIph component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
@Features.DisableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)
public class TabGridIphTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GROUPS_ANDROID, true);
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
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GROUPS_ANDROID, null);
    }

    @Test
    @MediumTest
    public void testShowAndHideIphDialog() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(
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
        CriteriaHelper.pollInstrumentationThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Restart chrome to verify that IPH message card is still there.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(
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
