// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests account picker bottomsheet of the web signin flow.
 *
 * TODO(https://crbug.com/1090356): Add render tests for bottomsheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
public class AccountPickerBottomSheetTest {
    @Rule
    public final Features.InstrumentationProcessor mProcessor =
            new Features.InstrumentationProcessor();

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    // Destroys the mock AccountManagerFacade in the end as ChromeActivity may needs
    // to unregister observers in the stub.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mActivityTestRule);

    @Mock
    private AccountPickerCoordinator.Listener mAccountPickerListenerMock;

    private final String mFullName1 = "Test Account1";
    private final String mAccountName1 = "test.account1@gmail.com";
    private final String mAccountName2 = "test.account2@gmail.com";
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinator;

    @Before
    public void setUp() {
        initMocks(this);
        addAccount(mAccountName1, mFullName1);
        addAccount(mAccountName2, "");
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(this::buildAndShowAccountPickerBottomSheet);
    }

    private void buildAndShowAccountPickerBottomSheet() {
        mAccountPickerBottomSheetCoordinator =
                new AccountPickerBottomSheetCoordinator(mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().getBottomSheetController(),
                        mAccountPickerListenerMock);
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
        verify(mAccountPickerListenerMock).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        onView(withText(mAccountName1)).check(matches(isDisplayed()));
        onView(withText(mFullName1)).perform(click());
        verify(mAccountPickerListenerMock).onAccountSelected(mAccountName1, true);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        onView(withText(mAccountName2)).perform(click());
        verify(mAccountPickerListenerMock).onAccountSelected(mAccountName2, false);
    }

    private void addAccount(String accountName, String fullName) {
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(accountName, profileData);
    }
}
