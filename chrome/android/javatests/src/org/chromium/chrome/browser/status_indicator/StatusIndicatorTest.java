// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorSceneLayer mStatusIndicatorSceneLayer;
    private View mStatusIndicatorContainer;
    private ViewGroup.MarginLayoutParams mControlContainerLayoutParams;

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

        assertThat("Wrong initial Android view visibility.",
                mStatusIndicatorContainer.getVisibility(), equalTo(View.GONE));
        Assert.assertFalse("Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        assertThat("Wrong initial control container top margin.",
                mControlContainerLayoutParams.topMargin, equalTo(0));

        TestThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::show);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // TODO(sinansahin): Investigate setting the duration for the browser controls animations to
        // 0 for testing.

        // Wait until the status indicator finishes animating, or becomes fully visible.
        CriteriaHelper.pollUiThread(Criteria.equals(mStatusIndicatorContainer.getHeight(),
                fullscreenManager::getTopControlsMinHeightOffset));

        // Now, the Android view should be visible.
        assertThat("Wrong Android view visibility.", mStatusIndicatorContainer.getVisibility(),
                equalTo(View.VISIBLE));

        TestThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::hide);

        // The Android view visibility should be {@link View.GONE} after #hide().
        assertThat("Wrong Android view visibility.", mStatusIndicatorContainer.getVisibility(),
                equalTo(View.GONE));

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                Criteria.equals(0, fullscreenManager::getTopControlsMinHeightOffset));

        Assert.assertFalse("Composited view shouldn't be visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
    }
}
