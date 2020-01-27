// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.pressBack;
import static android.support.test.espresso.matcher.RootMatchers.isDialog;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.settings.sync.AccountManagementFragment;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Test the lifecycle of sign-in and sign-out.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninSignoutIntegrationTest {
    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninUtils.Natives mSigninUtilsNativeMock;

    @Rule
    public final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Mock
    private SigninManager.SignInStateObserver mSignInStateObserverMock;

    private SigninManager mSigninManager;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninUtilsJni.TEST_HOOKS, mSigninUtilsNativeMock);
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSigninManager = IdentityServicesProvider.get().getSigninManager(); });
        mSigninManager.addSignInStateObserver(mSignInStateObserverMock);
    }

    @After
    public void tearDown() {
        mSigninManager.removeSignInStateObserver(mSignInStateObserverMock);
        SigninTestUtil.tearDownAuthForTest();
    }

    @Test
    @LargeTest
    public void testSignIn() {
        Account account = SigninTestUtil.addTestAccount();
        ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncher.get().launchActivity(
                            mActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS);
                });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Account should not be signed in!",
                    mSigninManager.getIdentityManager().hasPrimaryAccount());
            Assert.assertNull(mSigninManager.getIdentityManager().getPrimaryAccountInfo());
        });
        onView(withId(R.id.positive_button)).perform(click());
        verify(mSignInStateObserverMock).onSignedIn();
        verify(mSignInStateObserverMock, never()).onSignedOut();
        assertSignedIn();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(account,
                    mSigninManager.getIdentityManager().getPrimaryAccountInfo().getAccount());
        });
    }

    @Test
    @LargeTest
    public void testSignOut() {
        signIn();
        mActivityTestRule.startSettingsActivity(AccountManagementFragment.class.getName());
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Account should be signed out!",
                    mSigninManager.getIdentityManager().hasPrimaryAccount());
            Assert.assertNull(mSigninManager.getIdentityManager().getPrimaryAccountInfo());
        });
        verify(mSignInStateObserverMock).onSignedOut();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @LargeTest
    public void testSignOutDismissedByPressingBack() {
        signIn();
        mActivityTestRule.startSettingsActivity(AccountManagementFragment.class.getName());
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(isRoot()).perform(pressBack());
        verify(mSignInStateObserverMock, never()).onSignedOut();
        assertSignedIn();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @LargeTest
    public void testSignOutCancelled() {
        signIn();
        mActivityTestRule.startSettingsActivity(AccountManagementFragment.class.getName());
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        verify(mSignInStateObserverMock, never()).onSignedOut();
        assertSignedIn();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void signIn() {
        Account account = SigninTestUtil.addTestAccount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSigninManager.signIn(SigninAccessPoint.SETTINGS, account, null); });
        assertSignedIn();
        // TODO(https://crbug.com/1041815): Usage of ChromeSigninController should be removed later
        Assert.assertTrue(ChromeSigninController.get().isSignedIn());
    }

    private void assertSignedIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Account should be signed in!",
                    mSigninManager.getIdentityManager().hasPrimaryAccount());
        });
    }
}
