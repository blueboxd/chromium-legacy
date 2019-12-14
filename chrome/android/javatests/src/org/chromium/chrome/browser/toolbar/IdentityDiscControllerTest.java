// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;

/**
 * Instrumentation test for Identity Disc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityDiscControllerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() {
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        SigninTestUtil.tearDownAuthForTest();
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithNavigation() {
        // User is signed in.
        SigninTestUtil.addAndSignInTestAccount();

        // Identity Disc should be visible on NTP.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        View experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNotNull("IdentityDisc is not inflated", experimentalButton);
        Assert.assertEquals(
                "IdentityDisc is not visible", View.VISIBLE, experimentalButton.getVisibility());

        // Identity Disc should be hidden on navigation away from NTP.
        mActivityTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        Assert.assertEquals("IdentityDisc is still visible outside of NTP", View.GONE,
                experimentalButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithSignInState() {
        // When user is signed out, Identity Disc should not be visible.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        View experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNull("IdentityDisc is visible for signed out user", experimentalButton);

        // Identity Disc should be shown on sign-in state change without NTP refresh.
        SigninTestUtil.addAndSignInTestAccount();
        // Navigate away from NTP and back to trigger Identity Disc state change.
        mActivityTestRule.loadUrl(UrlConstants.CHROME_BLANK_URL);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        experimentalButton =
                mActivityTestRule.getActivity().findViewById(R.id.experimental_toolbar_button);
        Assert.assertNotNull("IdentityDisc is not inflated", experimentalButton);
        Assert.assertEquals(
                "IdentityDisc is not visible", View.VISIBLE, experimentalButton.getVisibility());
    }
}
