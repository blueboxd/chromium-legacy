// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeProfileDataSource;

/**
 * This test rule mocks AccountManagerFacade and manages sign-in/sign-out.
 *
 * When the user does not invoke any sign-in functions with this rule, the rule will not
 * invoke any native code, therefore it is safe to use it in Robolectric tests just as
 * a simple AccountManagerFacade mock.
 */
public class AccountManagerTestRule implements TestRule {
    public static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade;
    private boolean mIsSignedIn;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(null));
    }

    public AccountManagerTestRule(FakeProfileDataSource fakeProfileDataSource) {
        this(new FakeAccountManagerFacade(fakeProfileDataSource));
    }

    public AccountManagerTestRule(FakeAccountManagerFacade fakeAccountManagerFacade) {
        mFakeAccountManagerFacade = fakeAccountManagerFacade;
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpRule();
                try {
                    statement.evaluate();
                } finally {
                    tearDownRule();
                }
            }
        };
    }

    /**
     * Sets up the AccountManagerFacade mock.
     */
    public void setUpRule() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /**
     * Tears down the AccountManagerFacade mock and signs out if user is signed in.
     */
    public void tearDownRule() {
        if (mIsSignedIn) {
            signOut();
        }
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    /**
     * Add an account to the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(Account account) {
        mFakeAccountManagerFacade.addAccount(account);
        return account;
    }

    /**
     * Add an account of the given accountName to the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(String accountName) {
        return addAccount(AccountUtils.createAccountFromName(accountName));
    }

    /**
     * Add an account to the fake AccountManagerFacade and its profileData to the
     * ProfileDataSource of the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(String accountName, ProfileDataSource.ProfileData profileData) {
        Account account = addAccount(accountName);
        mFakeAccountManagerFacade.setProfileData(accountName, profileData);
        return account;
    }

    /**
     * Waits for the AccountTrackerService to seed system accounts.
     */
    public void waitForSeeding() {
        SigninTestUtil.seedAccounts();
    }

    /**
     * Adds an account and seed it in native code.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account addAccountAndWaitForSeeding(String accountName) {
        Account account = addAccount(accountName);
        waitForSeeding();
        return account;
    }

    /**
     * Removes an account and seed it in native code.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public void removeAccountAndWaitForSeeding(String accountName) {
        mFakeAccountManagerFacade.removeAccount(AccountUtils.createAccountFromName(accountName));
        waitForSeeding();
    }

    /**
     * Add and sign in an account with the default name.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account addAndSignInTestAccount() {
        assert !mIsSignedIn : "An account is already signed in!";
        Account account = addAccountAndWaitForSeeding(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signIn(account);
        mIsSignedIn = true;
        return account;
    }

    /**
     * Returns the currently signed in account.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account getCurrentSignedInAccount() {
        return SigninTestUtil.getCurrentAccount();
    }

    /**
     * Sign out from the current account.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public void signOut() {
        SigninTestUtil.signOut();
        mIsSignedIn = false;
    }
}
