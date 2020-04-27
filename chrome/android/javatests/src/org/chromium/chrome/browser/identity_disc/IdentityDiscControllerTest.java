// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;

/**
 * Instrumentation test for Identity Disc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityDiscControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;

    @Before
    public void setUp() {
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
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
        waitForView(allOf(withId(R.id.optional_toolbar_button), isDisplayed()));

        // Identity Disc should be hidden on navigation away from NTP.
        leaveNTP();
        onView(withId(R.id.optional_toolbar_button))
                .check(matches(anyOf(withEffectiveVisibility(ViewMatchers.Visibility.GONE),
                        not(withContentDescription(
                                R.string.accessibility_toolbar_btn_identity_disc)))));
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithSignInState() {
        // When user is signed out, Identity Disc should not be visible on the NTP.
        onView(withId(R.id.optional_toolbar_button)).check((view, noViewException) -> {
            if (view != null) {
                ViewMatchers.assertThat("IdentityDisc view should be gone if it exists",
                        view.getVisibility(), Matchers.is(View.GONE));
            }
        });

        // Identity Disc should be shown on sign-in state change without NTP refresh.
        SigninTestUtil.addAndSignInTestAccount();
        waitForView(allOf(withId(R.id.optional_toolbar_button), isDisplayed()));

        onView(withId(R.id.optional_toolbar_button))
                .check(matches(
                        withContentDescription(R.string.accessibility_toolbar_btn_identity_disc)));

        SigninTestUtil.removeTestAccount(SigninTestUtil.getCurrentAccount().name);
        waitForView(allOf(withId(R.id.optional_toolbar_button),
                withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    private void leaveNTP() {
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        ChromeTabUtils.waitForTabPageLoaded(mTab, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }
}
