// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.anyOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Tests {@link ShareButtonController}. */

@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(
        {ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR, ChromeFeatureList.START_SURFACE_ANDROID})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
public final class ShareButtonControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private boolean mButtonExpected;

    @Before
    public void setUp() {
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();

        int minimumWidthDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR, "minimum_width",
                ShareButtonController.MIN_WIDTH_DP);
        int deviceWidth =
                mActivityTestRule.getActivity().getResources().getConfiguration().screenWidthDp;

        mButtonExpected = deviceWidth > minimumWidthDp;
    }

    @After
    public void tearDown() {
        SigninTestUtil.tearDownAuthForTest();
    }

    @Test
    @MediumTest
    public void testShareButtonInToolbarIsDisabledOnStartNTP() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonView();
        if (experimentalButton != null) {
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);
            assertTrue("Share button isnt showing",
                    (View.GONE == experimentalButton.getVisibility()
                            || !shareString.equals(experimentalButton.getContentDescription())));
        }
    }

    @Test
    @MediumTest
    public void testShareButtonInToolbarIsEnabledOnBlankPage() {
        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonView();

        if (!mButtonExpected) {
            assertTrue(
                    experimentalButton == null || View.GONE == experimentalButton.getVisibility());
        } else {
            assertNotNull("experimental button not found", experimentalButton);
            assertEquals(View.VISIBLE, experimentalButton.getVisibility());
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertTrue(shareString.equals(experimentalButton.getContentDescription()));
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testShareButtonInToolbarNotAffectedByOverview() {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getStartSurface()
                                   .getController()
                                   .setOverviewState(OverviewModeState.SHOWING_START));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));

        View optionalButton = mActivityTestRule.getActivity()
                                      .getToolbarManager()
                                      .getToolbarLayoutForTesting()
                                      .getOptionalButtonView();
        if (!mButtonExpected) {
            assertTrue(optionalButton == null || View.GONE == optionalButton.getVisibility());
        } else {
            assertNotNull("optional button not found", optionalButton);

            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertEquals(shareString, optionalButton.getContentDescription());
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testShareButtonDisabledOnDataUrl() {
        mActivityTestRule.loadUrl("data:,Hello%2C%20World!");

        ViewUtils.waitForView(allOf(withId(R.id.optional_toolbar_button),
                anyOf(not(isDisplayed()), not(withContentDescription(R.string.share)))));

        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonView();
        if (experimentalButton != null) {
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);
            assertTrue("Share button isnt showing",
                    (View.GONE == experimentalButton.getVisibility()
                            || !shareString.equals(experimentalButton.getContentDescription())));
        }
    }

    // TODO(crbug/1036023) Add a test that checks that expected intents are fired.
}
