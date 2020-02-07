// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Utility class for sign-in functionalities in native Sync browser tests.
 */
public final class SyncTestSigninUtils {
    private static final String TAG = "SyncTestSigninUtils";

    /**
     * Signs in the test account.
     */
    private static void signinTestAccount(final Account account) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get().getSigninManager().signIn(
                    SigninAccessPoint.UNKNOWN, account, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            ProfileSyncService.get().setFirstSetupComplete(
                                    SyncFirstSetupCompleteSource.BASIC_FLOW);
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
        });
        Assert.assertEquals(account, SigninTestUtil.getCurrentAccount());
    }

    /**
     * Sets up the test account and signs in.
     */
    @CalledByNative
    private static Account setUpTestAccountAndSignIn() {
        Account account = SigninTestUtil.addTestAccount();
        signinTestAccount(account);
        return account;
    }

    /**
     * Sets up the test authentication environment.
     */
    @CalledByNative
    private static void setUpAuthForTest() {
        SigninTestUtil.setUpAuthForTest();
    }

    /**
     * Tears down the test authentication environment.
     */
    @CalledByNative
    private static void tearDownAuthForTest() {
        SigninTestUtil.tearDownAuthForTest();
    }
}
