// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static android.support.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Integration tests for status indicator covering related code in
 * {@link StatusIndicatorCoordinator} and {@link TabbedRootUiCoordinator}.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures({ChromeFeatureList.OFFLINE_INDICATOR_V2})
// TODO(crbug.com/1035584): Enable for tablets once we support them.
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
public class StatusIndicatorTest {
    // clang-format on

    @ClassRule
    public static DisableAnimationsTestRule mDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorSceneLayer mStatusIndicatorSceneLayer;
    private View mStatusIndicatorContainer;
    private ViewGroup.MarginLayoutParams mControlContainerLayoutParams;
    private ChromeFullscreenManager mFullscreenManager;

    @Before
    public void setUp() throws InterruptedException {
        TabbedRootUiCoordinator.setEnableStatusIndicatorForTests(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        mStatusIndicatorCoordinator = ((TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                                               .getRootUiCoordinatorForTesting())
                                              .getStatusIndicatorCoordinatorForTesting();
        mStatusIndicatorSceneLayer = mStatusIndicatorCoordinator.getSceneLayer();
        mStatusIndicatorContainer =
                mActivityTestRule.getActivity().findViewById(R.id.status_indicator);
        final View controlContainer =
                mActivityTestRule.getActivity().findViewById(R.id.control_container);
        mControlContainerLayoutParams =
                (ViewGroup.MarginLayoutParams) controlContainer.getLayoutParams();
        mFullscreenManager = mActivityTestRule.getActivity().getFullscreenManager();
    }

    @After
    public void tearDown() {
        TabbedRootUiCoordinator.setEnableStatusIndicatorForTests(false);
    }

    @Test
    @MediumTest
    public void testShowAndHide() {
        final ChromeFullscreenManager fullscreenManager =
                mActivityTestRule.getActivity().getFullscreenManager();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals("Wrong initial Android view visibility.", View.GONE,
                mStatusIndicatorContainer.getVisibility());
        Assert.assertFalse("Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        Assert.assertEquals("Wrong initial control container top margin.", 0,
                mControlContainerLayoutParams.topMargin);

        TestThreadUtils.runOnUiThreadBlocking(() -> mStatusIndicatorCoordinator.show(
        "Status", null, Color.BLACK, Color.WHITE, Color.WHITE));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // TODO(sinansahin): Investigate setting the duration for the browser controls animations to
        // 0 for testing.

        // Wait until the status indicator finishes animating, or becomes fully visible.
        CriteriaHelper.pollUiThread(Criteria.equals(mStatusIndicatorContainer.getHeight(),
                fullscreenManager::getTopControlsMinHeightOffset));

        // Now, the Android view should be visible.
        Assert.assertEquals("Wrong Android view visibility.", View.VISIBLE,
                mStatusIndicatorContainer.getVisibility());
        Assert.assertEquals("Wrong background color.", Color.BLACK,
                ((ColorDrawable) mStatusIndicatorContainer.getBackground()).getColor());

        TestThreadUtils.runOnUiThreadBlocking(() -> mStatusIndicatorCoordinator.updateContent(
                "Exit status", null, Color.WHITE, Color.BLACK, Color.BLACK, () -> {}));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The Android view should be visible.
        Assert.assertEquals("Wrong Android view visibility.", View.VISIBLE,
                mStatusIndicatorContainer.getVisibility());
        Assert.assertEquals("Wrong background color.", Color.WHITE,
                ((ColorDrawable) mStatusIndicatorContainer.getBackground()).getColor());

        TestThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::hide);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                Criteria.equals(0, fullscreenManager::getTopControlsMinHeightOffset));

        // The Android view visibility should be {@link View.GONE} after #hide().
        Assert.assertEquals("Wrong Android view visibility.", View.GONE,
                mStatusIndicatorContainer.getVisibility());

        Assert.assertFalse("Composited view shouldn't be visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    public void testShowAndHideOnStartSurface() {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view))
                .check(matches(isDisplayed()));
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        Assert.assertFalse("Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());

        TestThreadUtils.runOnUiThreadBlocking(() -> mStatusIndicatorCoordinator.show(
                "Status", null, Color.BLACK, Color.WHITE, Color.WHITE));

        // The status indicator will be immediately visible.
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(mStatusIndicatorContainer.getHeight())));
        onView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view))
                .check(matches(withTopMargin(mFullscreenManager.getTopControlsHeight())));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mStatusIndicatorCoordinator.updateContent("Exit status", null,
                        Color.WHITE, Color.BLACK, Color.BLACK, () -> {}));

        // #updateContent shouldn't change the layout.
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(mStatusIndicatorContainer.getHeight())));
        onView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view))
                .check(matches(withTopMargin(mFullscreenManager.getTopControlsHeight())));

        TestThreadUtils.runOnUiThreadBlocking(() -> mStatusIndicatorCoordinator.hide());

        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        onView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view))
                .check(matches(withTopMargin(mFullscreenManager.getTopControlsHeight())));
    }

    private static Matcher<View> withTopMargin(final int expected) {
        return new TypeSafeMatcher<View>() {
            private int mActual;
            @Override
            public boolean matchesSafely(final View view) {
                mActual = ((ViewGroup.MarginLayoutParams) view.getLayoutParams()).topMargin;
                return mActual == expected;
            }
            @Override
            public void describeTo(final Description description) {
                // TODO(sinansahin): This is a work-around because the message from
                // #describeMismatchSafely is ignored. If it is fixed one day, we can implement
                // #describeMismatchSafely.
                description.appendText("View should have a topMargin of " + expected)
                        .appendText(System.lineSeparator())
                        .appendText("but actually has " + mActual);
            }
        };
    }
}
