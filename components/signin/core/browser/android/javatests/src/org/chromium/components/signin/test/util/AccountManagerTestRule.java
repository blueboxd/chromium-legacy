// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.components.signin.AccountManagerFacade;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * JUnit4 rule for overriding behaviour of {@link AccountManagerFacade} for tests.
 */
public class AccountManagerTestRule implements TestRule {
    private FakeAccountManagerDelegate mDelegate;

    /**
     * Test method annotation signaling that the account population should be blocked until {@link
     * #unblockGetAccountsAndWaitForAccountsPopulated()} is called.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface BlockGetAccounts {}

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                final int profileDataSourceFlag =
                        FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE;
                final int blockGetAccountsFlag =
                        description.getAnnotation(BlockGetAccounts.class) == null
                        ? FakeAccountManagerDelegate.DISABLE_BLOCK_GET_ACCOUNTS
                        : FakeAccountManagerDelegate.ENABLE_BLOCK_GET_ACCOUNTS;
                mDelegate =
                        new FakeAccountManagerDelegate(profileDataSourceFlag, blockGetAccountsFlag);
                AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
                statement.evaluate();
            }
        };
    }

    /**
     * Unblocks all get accounts requests to {@link AccountManagerFacade}.
     * Has no effect in tests that are not annotated with {@link BlockGetAccounts}.
     */
    public void unblockGetAccountsAndWaitForAccountsPopulated() {
        mDelegate.unblockGetAccounts();
        AccountManagerFacade.get().tryGetGoogleAccounts();
    }
}
