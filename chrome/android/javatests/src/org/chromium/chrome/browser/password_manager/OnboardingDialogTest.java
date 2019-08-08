// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Test for the password manager onboarding modal dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OnboardingDialogTest {
    private OnboardingDialogCoordinator mDialog;
    private static final String TITLE = "Onboarding title!";
    private static final String DETAILS = "Explanation text.";

    @Mock
    private Callback<Boolean> mOnClick;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mDialog = new OnboardingDialogCoordinator(mActivityTestRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> mDialog.showDialog(TITLE, DETAILS, mOnClick));
    }

    @Test
    @SmallTest
    public void testDialogSubviewsData() {
        onView(withId(R.id.onboarding_title)).check(matches(withText(TITLE)));
        onView(withId(R.id.onboarding_details)).check(matches(withText(DETAILS)));
    }

    @Test
    @SmallTest
    public void testOkPressedCallback() {
        onView(withId(R.id.positive_button)).perform(click());
        verify(mOnClick).onResult(true);
    }

    @Test
    @SmallTest
    public void testCancelPressedCallback() {
        onView(withId(R.id.negative_button)).perform(click());
        verify(mOnClick).onResult(false);
    }
}
