// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.support.test.filters.MediumTest;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** End-to-end tests for TabGroupUi component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID})
public class TabGroupUiTest {
    // clang-format on

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderStrip_Select5thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        createTabs(cta, false, 10);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 5th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 4);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
        });
        mRenderTestRule.render(recyclerViewReference.get(), "5th_tab_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderStrip_Select10thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        createTabs(cta, false, 10);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 10th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 9);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
        });
        mRenderTestRule.render(recyclerViewReference.get(), "10th_tab_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderStrip_AddTab() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        createTabs(cta, false, 10);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the first tab in group and add one new tab to group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
            // Disable animation to reduce flakiness.
            stripRecyclerView.setItemAnimator(null);
        });
        onView(allOf(withId(R.id.toolbar_right_button), withParent(withId(R.id.main_content))))
                .perform(click());
        mRenderTestRule.render(recyclerViewReference.get(), "11th_tab_selected");
    }
}
