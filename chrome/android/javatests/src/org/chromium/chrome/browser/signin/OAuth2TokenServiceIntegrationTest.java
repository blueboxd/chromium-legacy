// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
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
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.OAuth2TokenService;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;

/**
 * Integration test for the OAuth2TokenService.
 *
 * These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class OAuth2TokenServiceIntegrationTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private static final Account TEST_ACCOUNT1 =
            AccountManagerFacade.createAccountFromName("foo@gmail.com");
    private static final Account TEST_ACCOUNT2 =
            AccountManagerFacade.createAccountFromName("bar@gmail.com");
    private static final AccountHolder TEST_ACCOUNT_HOLDER_1 =
            AccountHolder.builder(TEST_ACCOUNT1).alwaysAccept(true).build();
    private static final AccountHolder TEST_ACCOUNT_HOLDER_2 =
            AccountHolder.builder(TEST_ACCOUNT2).alwaysAccept(true).build();

    private OAuth2TokenService mOAuth2TokenService;
    private ChromeSigninController mChromeSigninController;

    @Before
    public void setUp() {
        mapAccountNamesToIds();

        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();

        // Make sure there is no account signed in yet.
        mChromeSigninController = ChromeSigninController.get();
        mChromeSigninController.setSignedInAccountName(null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Seed test accounts to AccountTrackerService.
            seedAccountTrackerService();

            // Get a reference to the service.
            mOAuth2TokenService = IdentityServicesProvider.getOAuth2TokenService();
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mChromeSigninController.setSignedInAccountName(null);
            mOAuth2TokenService.updateAccountList();
        });
        SigninHelper.resetSharedPrefs();
        SigninTestUtil.resetSigninState();
    }

    private void mapAccountNamesToIds() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
                @Override
                public String getAccountId(String accountName) {
                    return "gaia-id-" + accountName;
                }

                @Override
                public boolean canBeUsed() {
                    return true;
                }
            });
        });
    }

    private void seedAccountTrackerService() {
        AccountIdProvider provider = AccountIdProvider.getInstance();
        String[] accountNames = {TEST_ACCOUNT1.name, TEST_ACCOUNT2.name};
        String[] accountIds = {
                provider.getAccountId(accountNames[0]), provider.getAccountId(accountNames[1])};
        IdentityServicesProvider.getAccountTrackerService().syncForceRefreshForTest(
                accountIds, accountNames);
    }

    private void addAccount(AccountHolder accountHolder) {
        mAccountManagerTestRule.addAccount(accountHolder);
        TestThreadUtils.runOnUiThreadBlocking(this::seedAccountTrackerService);
    }

    private void removeAccount(AccountHolder accountHolder) {
        mAccountManagerTestRule.removeAccount(accountHolder);
        TestThreadUtils.runOnUiThreadBlocking(this::seedAccountTrackerService);
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredAndNoSignedInUser() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertArrayEquals("Initial state: getAccounts must be empty", new String[] {},
                    OAuth2TokenService.getAccounts());

            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals("No account: getAccounts must be empty", new String[] {},
                    OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals("No signed in account: getAccounts must be empty",
                    new String[] {}, OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals("Signed in: one account should be available",
                    new String[] {AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name)},
                    OAuth2TokenService.getAccounts());

        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedInOther() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT2.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals(
                    "Signed in but different account, getAccounts must remain empty",
                    new String[] {}, OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        addAccount(TEST_ACCOUNT_HOLDER_1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run one validation.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals("Signed in and one account available",
                    new String[] {AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name)},
                    OAuth2TokenService.getAccounts());
        });

        // Add another account.
        addAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-run validation.
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<String>(Arrays.asList(
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name),
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT2.name))),
                    new HashSet<String>(Arrays.asList(OAuth2TokenService.getAccounts())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveOne() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run one validation.
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<String>(Arrays.asList(
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name),
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT2.name))),
                    new HashSet<String>(Arrays.asList(OAuth2TokenService.getAccounts())));
        });

        removeAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals(
                    "Only one account available, account2 should not be returned anymore",
                    new String[] {AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name)},
                    OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<String>(Arrays.asList(
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name),
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT2.name))),
                    new HashSet<String>(Arrays.asList(OAuth2TokenService.getAccounts())));
        });

        // Remove all.
        removeAccount(TEST_ACCOUNT_HOLDER_1);
        removeAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mOAuth2TokenService.updateAccountList();

            Assert.assertArrayEquals(
                    "No account available", new String[] {}, OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    @RetryOnFailure
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<String>(Arrays.asList(
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name),
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT2.name))),
                    new HashSet<String>(Arrays.asList(OAuth2TokenService.getAccounts())));

            // Remove all.
            mChromeSigninController.setSignedInAccountName(null);
        });

        removeAccount(TEST_ACCOUNT_HOLDER_1);
        removeAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Not signed in and no accounts available", new String[] {},
                    OAuth2TokenService.getAccounts());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        addAccount(TEST_ACCOUNT_HOLDER_1);
        addAccount(TEST_ACCOUNT_HOLDER_2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<String>(Arrays.asList(
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT1.name),
                            AccountIdProvider.getInstance().getAccountId(TEST_ACCOUNT2.name))),
                    new HashSet<String>(Arrays.asList(OAuth2TokenService.getAccounts())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredButSignedIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Mark user as signed in without setting up the account.
            mChromeSigninController.setSignedInAccountName(TEST_ACCOUNT1.name);

            // Run test.
            mOAuth2TokenService.updateAccountList();

            Assert.assertEquals(
                    "No accounts available", new String[] {}, OAuth2TokenService.getAccounts());
        });
    }
}
