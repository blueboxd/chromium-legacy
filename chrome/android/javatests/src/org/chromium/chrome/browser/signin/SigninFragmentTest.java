// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.io.IOException;

/**
 * Render tests for signin fragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFragmentTest {
    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule
    public final RenderTestRule mRenderTestRule = new RenderTestRule();

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNewAccount() throws IOException {
        View view = getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoAddAccountFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER);
        });
        mRenderTestRule.render(view, "signin_fragment_new_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccount() throws IOException {
        Account account = mSyncTestRule.setUpTestAccount();
        View view = getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoChooseAccountFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER, account.name);
        });
        mRenderTestRule.render(view, "signin_fragment_not_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentDefaultAccount() throws IOException {
        Account account = mSyncTestRule.setUpTestAccount();
        View view = getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoDefaultFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER, account.name);
        });
        mRenderTestRule.render(view, "signin_fragment_default_account");
    }

    @Test
    @MediumTest
    public void testSigninFragmentWithDefaultFlow() {
        SigninActivityLauncher.get().launchActivity(
                mSyncTestRule.getActivity(), SigninAccessPoint.SETTINGS);
        onView(withId(R.id.positive_button)).check(matches(withText(R.string.signin_add_account)));
        onView(withId(R.id.negative_button)).check(matches(withText(R.string.cancel)));
    }

    private View getSigninFragmentViewAfterStartingActivity(Runnable activityTrigger) {
        SigninActivity signinActivity =
                ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                        SigninActivity.class, activityTrigger);
        return signinActivity.findViewById(R.id.fragment_container);
    }
}
