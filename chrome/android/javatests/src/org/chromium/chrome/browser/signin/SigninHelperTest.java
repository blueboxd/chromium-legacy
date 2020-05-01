// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;

/**
 * Instrumentation tests for {@link SigninHelper}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SigninHelperTest {
    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private MockChangeEventChecker mEventChecker;

    @Before
    public void setUp() {
        mEventChecker = new MockChangeEventChecker();
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testSimpleAccountRename() {
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("B", getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    public void testNotSignedInAccountRename() {
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertNull(getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    public void testSimpleAccountRenameTwice() {
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("B", getNewSignedInAccountName());
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mEventChecker, "B");
        Assert.assertEquals("C", getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testNotSignedInAccountRename2() {
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertNull(getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testChainedAccountRename2() {
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("D", getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoopedAccountRename() {
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        mEventChecker.insertRenameEvent("D", "A"); // Looped.
        Account account = AccountUtils.createAccountFromName("D");
        AccountHolder accountHolder = AccountHolder.builder(account).build();
        mAccountManagerTestRule.addAccount(accountHolder);
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("D", getNewSignedInAccountName());
    }

    private String getNewSignedInAccountName() {
        return SigninPreferencesManager.getInstance().getNewSignedInAccountName();
    }
}
