// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.chrome.browser.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests account picker bottom sheet of the web signin flow.
 *
 * TODO(https://crbug.com/1090356): Add render tests for bottomsheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
public class AccountPickerBottomSheetTest {
    private static class CustomFakeProfileDataSource extends FakeProfileDataSource {
        int getNumberOfObservers() {
            return mObservers.size();
        }
    }

    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String ACCOUNT_NAME1 = "test.account1@gmail.com";
    private static final String ACCOUNT_NAME2 = "test.account2@gmail.com";

    @Rule
    public final Features.InstrumentationProcessor mProcessor =
            new Features.InstrumentationProcessor();

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final CustomFakeProfileDataSource mFakeProfileDataSource =
            new CustomFakeProfileDataSource();

    private final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeProfileDataSource);

    // Destroys the mock AccountManagerFacade in the end as ChromeActivity may needs
    // to unregister observers in the stub.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mActivityTestRule);

    @Mock
    private AccountPickerDelegate mAccountPickerDelegateMock;

    @Before
    public void setUp() {
        initMocks(this);
        addAccount(ACCOUNT_NAME1, FULL_NAME1, GIVEN_NAME1);
        addAccount(ACCOUNT_NAME2, null, null);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void buildAndShowAccountPickerBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountPickerBottomSheetCoordinator accountPickerBottomSheetCoordinator =
                    new AccountPickerBottomSheetCoordinator(mActivityTestRule.getActivity(),
                            getBottomSheetController(), mAccountPickerDelegateMock);
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithAccount() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
        onView(withText(ACCOUNT_NAME1)).check(matches(isDisplayed()));
        onView(withText(FULL_NAME1)).check(matches(isDisplayed()));
        onView(withId(R.id.account_selection_mark)).check(matches(isDisplayed()));
        String continueAsText = mActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(ACCOUNT_NAME2)).check(doesNotExist());
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        // Check expanded bottom sheet
        onView(allOf(withText(ACCOUNT_NAME1), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(allOf(withText(FULL_NAME1), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(withText(ACCOUNT_NAME2)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithZeroAccount() {
        // As we have already added accounts in our current AccountManagerFacade mock
        // Here since we want to test a zero account case, we would like to set up
        // a new AccountManagerFacade mock with no account in it. The mock will be
        // torn down in the end of the test in AccountManagerTestRule.
        AccountManagerFacadeProvider.setInstanceForTests(
                new FakeAccountManagerFacade(mFakeProfileDataSource));
        buildAndShowAccountPickerBottomSheet();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(ACCOUNT_NAME1)).check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeProfileDataSource.getNumberOfObservers());
        onView(isRoot()).perform(pressBack());
        Assert.assertFalse(controller.isSheetOpen());
        Assert.assertEquals(0, mFakeProfileDataSource.getNumberOfObservers());
    }

    @Test
    @MediumTest
    public void testDismissExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeProfileDataSource.getNumberOfObservers());
        onView(isRoot()).perform(pressBack());
        Assert.assertFalse(controller.isSheetOpen());
        Assert.assertEquals(0, mFakeProfileDataSource.getNumberOfObservers());
    }

    @Test
    @MediumTest
    public void testAccountDisappearedInCollapsedSheet() {
        buildAndShowAccountPickerBottomSheet();
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME1);
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME2);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountDisappearedInExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME1);
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME2);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountReappearedInCollapsedSheet() {
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME1);
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(ACCOUNT_NAME2);
        buildAndShowAccountPickerBottomSheet();
        checkZeroAccountBottomSheet();

        mAccountManagerTestRule.addAccount(ACCOUNT_NAME1);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        onView(withText(ACCOUNT_NAME1)).check(matches(isDisplayed()));
        onView(withText(FULL_NAME1)).check(matches(isDisplayed()));
        String continueAsText = mActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testProfileDataUpdateInExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        String newFullName = "New Full Name1";
        String newGivenName = "New Given Name1";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileDataSource.setProfileData(ACCOUNT_NAME1,
                    new ProfileDataSource.ProfileData(
                            ACCOUNT_NAME1, null, newFullName, newGivenName));
        });
        onView(allOf(withText(newFullName), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        // Check that profile data update when the bottom sheet is expanded won't
        // toggle out any hidden part.
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet() {
        buildAndShowAccountPickerBottomSheet();
        String continueAsText = mActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).perform(click());
        verify(mAccountPickerDelegateMock).signIn(ACCOUNT_NAME1);
    }

    @Test
    @MediumTest
    public void testSignInAnotherAccount() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        onView(withText(ACCOUNT_NAME2)).perform(click());
        String continueAsText = mActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, ACCOUNT_NAME2);
        onView(withText(continueAsText)).perform(click());
        verify(mAccountPickerDelegateMock).signIn(ACCOUNT_NAME2);
    }

    @Test
    @MediumTest
    public void testAddAccountOnExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        onView(withText(R.string.signin_add_account_to_device)).perform(click());
        verify(mAccountPickerDelegateMock).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectAnotherAccountOnExpandedSheet() {
        buildAndShowAccountPickerBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
        onView(withText(ACCOUNT_NAME2)).perform(click());

        onView(allOf(withText(ACCOUNT_NAME2), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        String continueAsText = mActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, ACCOUNT_NAME2);
        onView(withText(continueAsText)).check(matches(isDisplayed()));

        onView(allOf(withText(ACCOUNT_NAME1), withEffectiveVisibility(VISIBLE)))
                .check(doesNotExist());
        onView(allOf(withText(FULL_NAME1), withEffectiveVisibility(VISIBLE))).check(doesNotExist());
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    private void addAccount(String accountName, String fullName, String givenName) {
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, givenName);
        mAccountManagerTestRule.addAccount(accountName, profileData);
    }

    private void checkZeroAccountBottomSheet() {
        onView(allOf(withText(ACCOUNT_NAME1), withEffectiveVisibility(VISIBLE)))
                .check(doesNotExist());
        onView(allOf(withText(ACCOUNT_NAME2), withEffectiveVisibility(VISIBLE)))
                .check(doesNotExist());
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(allOf(withText(R.string.signin_add_account_to_device),
                       withEffectiveVisibility(VISIBLE)))
                .perform(click());
        verify(mAccountPickerDelegateMock).addAccount();
    }

    private BottomSheetController getBottomSheetController() {
        return mActivityTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }
}
