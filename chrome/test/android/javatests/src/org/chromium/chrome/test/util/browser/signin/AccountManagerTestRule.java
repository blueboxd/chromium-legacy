// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeProfileDataSource;

/**
 * JUnit4 rule for overriding behaviour of {@link AccountManagerFacade} for tests.
 */
public class AccountManagerTestRule implements TestRule {
    private final FakeAccountManagerFacade mFakeAccountManagerFacade;

    public AccountManagerTestRule(@Nullable FakeProfileDataSource fakeProfileDataSource) {
        mFakeAccountManagerFacade = new FakeAccountManagerFacade(fakeProfileDataSource);
    }

    public AccountManagerTestRule() {
        this(null);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
                try {
                    statement.evaluate();
                } finally {
                    AccountManagerFacadeProvider.resetInstanceForTests();
                }
            }
        };
    }

    /**
     * Sets the boolean for whether the account cache has already been populated.
     */
    public void setIsCachePopulated(boolean isCachePopulated) {
        mFakeAccountManagerFacade.setIsCachePopulated(isCachePopulated);
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
}
