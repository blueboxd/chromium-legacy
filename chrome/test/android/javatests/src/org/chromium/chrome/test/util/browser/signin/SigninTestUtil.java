// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;

import androidx.annotation.WorkerThread;

import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

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
        AccountManagerFacade.overrideAccountManagerFacadeForTests(sAccountManager);
        resetSigninState();
    }

    /**
     * Tears down the test authentication environment.
     */
    @WorkerThread
    public static void tearDownAuthForTest() {
        for (AccountHolder accountHolder : sAddedAccounts) {
            sAccountManager.removeAccountHolderBlocking(accountHolder);
        }
        sAddedAccounts.clear();
        resetSigninState();
    }

    /**
     * Returns the currently signed in account.
     */
    public static Account getCurrentAccount() {
        return ChromeSigninController.get().getSignedInUser();
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
        Account account = AccountManagerFacade.createAccountFromName(name);
        AccountHolder accountHolder = AccountHolder.builder(account).alwaysAccept(true).build();
        sAccountManager.addAccountHolderBlocking(accountHolder);
        sAddedAccounts.add(accountHolder);
        seedAccounts();
        return account;
    }

    /**
     * Add and sign in an account with the default name.
     */
    public static Account addAndSignInTestAccount() {
        Account account = addTestAccount(DEFAULT_ACCOUNT);
        ChromeSigninController.get().setSignedInAccountName(DEFAULT_ACCOUNT);
        seedAccounts();
        return account;
    }

    private static void seedAccounts() {
        Account[] accounts = sAccountManager.getAccountsSyncNoThrow();
        String[] accountNames = new String[accounts.length];
        String[] accountIds = new String[accounts.length];
        for (int i = 0; i < accounts.length; i++) {
            accountNames[i] = accounts[i].name;
            accountIds[i] = sAccountManager.getAccountGaiaId(accounts[i].name);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get().getAccountTrackerService().syncForceRefreshForTest(
                    accountIds, accountNames);
        });
    }

    /**
     * Should be called at setUp and tearDown so that the signin state is not leaked across tests.
     * The setUp call is implicit inside the constructor.
     */
    public static void resetSigninState() {
        // Clear cached signed account name and accounts list.
        ChromeSigninController.get().setSignedInAccountName(null);

        SigninPreferencesManager.getInstance().clearAccountsStateSharedPrefsForTesting();
    }

    private SigninTestUtil() {}
}
