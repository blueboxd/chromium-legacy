// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.Espresso.pressBack;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.filters.MediumTest;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;

/** Integration tests of the {@link StartSurface}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
public class StartSurfaceTest {
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:start_surface_variation";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        FeatureUtilities.setStartSurfaceEnabledForTesting(true);
        mActivityTestRule.startMainActivityFromLauncher();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/tasksonly"})
    public void testShowAndHideTasksOnlySurface() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_TABSWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        onView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/single"})
    public void testShowAndHideSingleSurface() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_HOMEPAGE));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        onView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_title))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container))
                .check(matches(isDisplayed()));

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1025296): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity()
                                       .findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
        } catch (ExecutionException e) {
            assertTrue(false);
        }
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r,
                    withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
        });

        pressBack();
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r,
                    withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({BASE_PARAMS + "/twopanes"})
    public void testShowAndHideTwoPanesSurface() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_TABSWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        onView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.start_surface.R.id.ss_bottom_bar))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body))
                .check(matches(isDisplayed()));

        onView(withId(org.chromium.chrome.start_surface.R.id.ss_explore_tab)).perform(click());
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r,
                    withId(org.chromium.chrome.start_surface.R.id.start_surface_explore_view));
        });

        pressBack();
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r,
                    withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
        assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }
}

// TODO(crbug.com/1033909): Add more integration tests.
