// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.isDialog;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

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
    public final ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

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
    public void testSigninFragmentNotDefaultAccountWithPrimaryAccount() throws IOException {
        Account account = mSyncTestRule.setUpTestAccount();
        SigninTestUtil.addTestAccount("test.second.account@gmail.com");
        View view = getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoChooseAccountFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER, account.name);
        });
        mRenderTestRule.render(view, "signin_fragment_choose_primary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccountWithSecondaryAccount() throws IOException {
        mSyncTestRule.setUpTestAccount();
        String secondAccountName = "test.second.account@gmail.com";
        SigninTestUtil.addTestAccount(secondAccountName);
        View view = getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoChooseAccountFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                    secondAccountName);
        });
        mRenderTestRule.render(view, "signin_fragment_choose_secondary_account");
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
    @LargeTest
    public void testClickingSettingsDoesNotSetFirstSetupComplete() throws Exception {
        Account account = mSyncTestRule.setUpTestAccount();
        SigninActivityLauncher.get().launchActivityForPromoDefaultFlow(
                mSyncTestRule.getActivity(), SigninAccessPoint.SETTINGS, account.name);
        onView(withText(account.name)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_details_description)).perform(clickOnClickableSpan());
        // Wait for sign in process to finish.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return IdentityServicesProvider.get()
                        .getSigninManager()
                        .getIdentityManager()
                        .hasPrimaryAccount();
            }
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // TODO(https://crbug.com/1041815): Usage of ChromeSigninController should be removed later
        Assert.assertTrue(ChromeSigninController.get().isSignedIn());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(ProfileSyncService.get().isFirstSetupComplete()); });
    }

    @Test
    @MediumTest
    public void testSigninFragmentWithDefaultFlow() {
        SigninActivityLauncher.get().launchActivity(
                mSyncTestRule.getActivity(), SigninAccessPoint.SETTINGS);
        onView(withId(R.id.positive_button)).check(matches(withText(R.string.signin_add_account)));
        onView(withId(R.id.negative_button)).check(matches(withText(R.string.cancel)));
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccountInAccountPickerDialog() {
        Account defaultAccount = mSyncTestRule.setUpTestAccount();
        String nonDefaultAccountName = "test.account.nondefault@gmail.com";
        SigninTestUtil.addTestAccount(nonDefaultAccountName);
        getSigninFragmentViewAfterStartingActivity(() -> {
            SigninActivityLauncher.get().launchActivityForPromoDefaultFlow(
                    mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                    defaultAccount.name);
        });
        onView(withText(defaultAccount.name)).check(matches(isDisplayed())).perform(click());
        onView(withText(nonDefaultAccountName)).inRoot(isDialog()).perform(click());
        onView(withText(nonDefaultAccountName)).check(matches(isDisplayed()));
    }

    private View getSigninFragmentViewAfterStartingActivity(Runnable activityTrigger) {
        SigninActivity signinActivity =
                ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                        SigninActivity.class, activityTrigger);
        return signinActivity.findViewById(R.id.fragment_container);
    }

    private ViewAction clickOnClickableSpan() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the one and only clickable span in the view";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length == 0) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                Assert.assertEquals("There should be only one clickable link", 1, spans.length);
                spans[0].onClick(view);
            }
        };
    }
}
