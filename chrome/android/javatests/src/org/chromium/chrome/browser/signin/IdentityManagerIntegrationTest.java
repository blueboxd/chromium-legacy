// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;

/**
 * Integration test for the IdentityManager.
 *
 * These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class IdentityManagerIntegrationTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    private static final String TEST_ACCOUNT1 = "foo@gmail.com";
    private static final String TEST_ACCOUNT2 = "bar@gmail.com";

    private CoreAccountInfo mTestAccount1;
    private CoreAccountInfo mTestAccount2;

    private IdentityMutator mIdentityMutator;
    private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        SigninTestUtil.setUpAuthForTest();

        mTestAccount1 = createCoreAccountInfoFromEmail(TEST_ACCOUNT1);
        mTestAccount2 = createCoreAccountInfoFromEmail(TEST_ACCOUNT2);

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();

        // Make sure there is no account signed in yet.
        ChromeSigninController.get().setSignedInAccountName(null);

        SigninTestUtil.seedAccounts();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator =
                    IdentityServicesProvider.get().getSigninManager().getIdentityMutator();
            mIdentityManager = IdentityServicesProvider.get().getIdentityManager();
        });
    }

    private static CoreAccountInfo createCoreAccountInfoFromEmail(String accountEmail) {
        String accountGaiaId =
                AccountManagerFacadeProvider.getInstance().getAccountGaiaId(accountEmail);
        return new CoreAccountInfo(new CoreAccountId(accountGaiaId), accountEmail, accountGaiaId);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null); });
        SigninTestUtil.tearDownAuthForTest();

        // TODO(https://crbug.com/1046412): Remove this.
        ChromeSigninController.get().setSignedInAccountName(null);
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredAndNoSignedInUser() {
        Assert.assertArrayEquals("Initial state: getAccounts must be empty",
                new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("No account: getAccounts must be empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("No signed in account: getAccounts must be empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("Signed in: one account should be available",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedInOther() {
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount2.getId());

            Assert.assertArrayEquals(
                    "Signed in but different account, getAccounts must remain empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run one validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("Signed in and one account available",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });

        // Add another account.
        SigninTestUtil.addTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-run validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveOne() {
        // Add accounts.
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.addTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run one validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        SigninTestUtil.removeTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals(
                    "Only one account available, account2 should not be returned anymore",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.addTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        // Remove all.
        SigninTestUtil.removeTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.removeTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("No account available", new CoreAccountInfo[] {},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    @RetryOnFailure
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.addTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        SigninTestUtil.removeTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.removeTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("Not signed in and no accounts available",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        SigninTestUtil.addTestAccount(TEST_ACCOUNT1);
        SigninTestUtil.addTestAccount(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredButSignedIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("No accounts available", new CoreAccountInfo[] {},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }
}
