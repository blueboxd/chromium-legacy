// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;

import androidx.annotation.WorkerThread;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerFacadeImpl;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Utility class for test signin functionality.
 */
public final class SigninTestUtil {
    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    @SuppressLint("StaticFieldLeak")
    private static FakeAccountManagerDelegate sAccountManager;
    @SuppressLint("StaticFieldLeak")
    private static List<AccountHolder> sAddedAccounts = new ArrayList<>();

    /**
     * Sets up the test authentication environment.
     *
     * This must be called before native is loaded.
     */
    @WorkerThread
    public static void setUpAuthForTest() {
        sAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.setInstanceForTests(
                    new AccountManagerFacadeImpl(sAccountManager));
        });
    }

    /**
     * Tears down the test authentication environment.
     */
    @WorkerThread
    public static void tearDownAuthForTest() {
        if (getCurrentAccount() != null) {
            signOut();
        }
        for (AccountHolder accountHolder : sAddedAccounts) {
            sAccountManager.removeAccountHolderBlocking(accountHolder);
        }
        sAddedAccounts.clear();

        // TODO(https://crbug.com/1046412): Remove this.
        // Clear cached signed account name.
        ChromeSigninController.get().setSignedInAccountName(null);

        SigninPreferencesManager.getInstance().clearAccountsStateSharedPrefsForTesting();
    }

    /**
     * Returns the currently signed in account.
     */
    public static Account getCurrentAccount() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return CoreAccountInfo.getAndroidAccountFrom(
                    IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                            ConsentLevel.SYNC));
        });
    }

    /**
     * Add an account with the default name.
     */
    public static Account addTestAccount() {
        return addTestAccount(DEFAULT_ACCOUNT);
    }

    /**
     * Add an account with a given name.
     */
    public static Account addTestAccount(String name) {
        Account account = AccountUtils.createAccountFromName(name);
        AccountHolder accountHolder = AccountHolder.builder(account).alwaysAccept(true).build();
        sAccountManager.addAccountHolderBlocking(accountHolder);
        sAddedAccounts.add(accountHolder);
        seedAccounts();
        return account;
    }

    /**
     * Remove an account with a given name.
     */
    public static void removeTestAccount(String name) {
        Account account = AccountUtils.createAccountFromName(name);
        AccountHolder accountHolder = AccountHolder.builder(account).alwaysAccept(true).build();
        sAccountManager.removeAccountHolderBlocking(accountHolder);
        sAddedAccounts.remove(accountHolder);
        seedAccounts();
    }

    /**
     * Add and sign in an account with the default name.
     */
    public static Account addAndSignInTestAccount() {
        Account account = addTestAccount(DEFAULT_ACCOUNT);
        signIn(account);
        return account;
    }
    /**
     * Sign into an account. Account should be added by {@link #addTestAccount} first.
     */
    public static void signIn(Account account) {
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager();
            signinManager.onFirstRunCheckDone(); // Allow sign-in
            signinManager.signIn(
                    SigninAccessPoint.UNKNOWN, account, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            ProfileSyncService.get().setFirstSetupComplete(
                                    SyncFirstSetupCompleteSource.BASIC_FLOW);
                            callbackHelper.notifyCalled();
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(account.name,
                    IdentityServicesProvider.get()
                            .getIdentityManager()
                            .getPrimaryAccountInfo(ConsentLevel.SYNC)
                            .getEmail());
        });
    }

    /**
     * Waits for the AccountTrackerService to seed system accounts.
     */
    public static void seedAccounts() {
        ThreadUtils.assertOnBackgroundThread();
        CallbackHelper ch = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountTrackerService accountTrackerService =
                    IdentityServicesProvider.get().getAccountTrackerService();
            if (accountTrackerService.checkAndSeedSystemAccounts()) {
                ch.notifyCalled();
            } else {
                AccountTrackerService.OnSystemAccountsSeededListener listener =
                        new AccountTrackerService.OnSystemAccountsSeededListener() {
                            @Override
                            public void onSystemAccountsSeedingComplete() {
                                accountTrackerService.removeSystemAccountsSeededListener(this);
                                ch.notifyCalled();
                            }
                        };
                accountTrackerService.addSystemAccountsSeededListener(listener);
            }
        });
        try {
            ch.waitForFirst("Timed out while waiting for system accounts to seed.");
        } catch (TimeoutException ex) {
            throw new RuntimeException("Timed out while waiting for system accounts to seed.");
        }
    }

    private static void signOut() {
        ThreadUtils.assertOnBackgroundThread();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get().getSigninManager().signOut(
                    SignoutReason.SIGNOUT_TEST, callbackHelper::notifyCalled, false);
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
    }

    private SigninTestUtil() {}
}
