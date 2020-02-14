// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Tests {@link ShareButtonController}. */

@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
public final class ShareButtonControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        CachedFeatureFlags.setStartSurfaceEnabledForTesting(true);
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();
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
                                          .getExperimentalButtonView();
        assertNotNull("experimental button not found", experimentalButton);
        assertEquals(View.GONE, experimentalButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testShareButtonInToolbarIsEnabledOnBlankPage() {
        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getExperimentalButtonView();

        assertNotNull("experimental button not found", experimentalButton);
        assertEquals(View.VISIBLE, experimentalButton.getVisibility());
        String shareString =
                mActivityTestRule.getActivity().getResources().getString(R.string.share);

        assertTrue(shareString.equals(experimentalButton.getContentDescription()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testShareButtonInToolbarIsDisabledWithOverview() {
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

        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getExperimentalButtonView();
        assertNotNull("experimental button not found", experimentalButton);

        String shareString =
                mActivityTestRule.getActivity().getResources().getString(R.string.share);

        assertNotEquals(shareString, experimentalButton.getContentDescription());
    }

    // TODO(crbug/1036023) Add a test that checks that expected intents are fired.
}
