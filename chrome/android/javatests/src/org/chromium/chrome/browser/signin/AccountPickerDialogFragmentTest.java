// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.accounts.Account;
import android.support.v4.app.FragmentManager;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Render tests for {@link AccountPickerDialogFragment}.
 * TODO(https://crbug.com/1032488):
 * Use FragmentScenario to test this fragment once we start using fragment from androidx.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountPickerDialogFragmentTest extends DummyUiActivityTestCase {
    @Spy
    private DummyAccountPickerTargetFragment mTargetFragmentMock =
            new DummyAccountPickerTargetFragment();

    private final String mFullName1 = "Test Account1";

    private final String mAccountName1 = "test.account1@gmail.com";

    private final String mAccountName2 = "test.account2@gmail.com";

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);

    @Before
    public void setUp() {
        initMocks(this);
        addAccount(mAccountName1, mFullName1);
        addAccount(mAccountName2, "");
        FragmentManager fragmentManager = getActivity().getSupportFragmentManager();
        fragmentManager.beginTransaction().add(mTargetFragmentMock, "Parent").commit();
        AccountPickerDialogFragment dialog = AccountPickerDialogFragment.create(mAccountName1);
        dialog.setTargetFragment(mTargetFragmentMock, 0);
        dialog.show(fragmentManager, "");
    }

    @Test
    @MediumTest
    public void testTitle() {
        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        onView(withText(R.string.signin_add_account)).perform(click());
        verify(mTargetFragmentMock).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        onView(withText(mAccountName1)).check(matches(isDisplayed()));
        onView(withText(mFullName1)).perform(click());
        verify(mTargetFragmentMock).onAccountSelected(mAccountName1, true);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        onView(withText(mAccountName2)).perform(click());
        verify(mTargetFragmentMock).onAccountSelected(mAccountName2, false);
    }

    private void addAccount(String accountName, String fullName) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder = AccountHolder.builder(account).alwaysAccept(true);
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(accountHolder.build(), profileData);
    }
}
