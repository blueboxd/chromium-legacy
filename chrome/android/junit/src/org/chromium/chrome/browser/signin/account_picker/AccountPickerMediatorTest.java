// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests the class {@link AccountPickerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountPickerMediatorTest {
    private static final String FULL_NAME1 = "Test Account1";
    private static final String ACCOUNT_NAME1 = "test.account1@gmail.com";
    private static final String ACCOUNT_NAME2 = "test.account2@gmail.com";

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Mock
    private AccountPickerCoordinator.Listener mListenerMock;

    private final MVCListAdapter.ModelList mModelList = new MVCListAdapter.ModelList();

    private AccountPickerMediator mMediator;

    @Before
    public void setUp() {
        initMocks(this);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    public void testModelPopulatedWhenStarted() {
        addAccount(ACCOUNT_NAME1, FULL_NAME1);
        addAccount(ACCOUNT_NAME2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_NAME1);
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(0, ACCOUNT_NAME1, FULL_NAME1, /* isSelectedAccount= */ true);
        checkItemForExistingAccountRow(1, ACCOUNT_NAME2, "", /* isSelectedAccount= */ false);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testModelUpdatedAfterSetSelectedAccountName() {
        addAccount(ACCOUNT_NAME1, FULL_NAME1);
        addAccount(ACCOUNT_NAME2, "");
        mMediator = new AccountPickerMediator(
                RuntimeEnvironment.application, mModelList, mListenerMock, ACCOUNT_NAME1);
        mMediator.setSelectedAccountName(ACCOUNT_NAME2);
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(
                0, ACCOUNT_NAME1, FULL_NAME1, /* isSelectedAccount= */ false);
        checkItemForExistingAccountRow(1, ACCOUNT_NAME2, "", /* isSelectedAccount= */ true);
        checkItemForAddAccountRow(2);
    }

    private void checkItemForExistingAccountRow(
            int position, String accountEmail, String fullName, boolean isSelectedAccount) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW, item.type);
        PropertyModel model = item.model;
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        Assert.assertEquals(accountEmail, profileData.getAccountName());
        Assert.assertEquals(fullName, profileData.getFullName());
        Assert.assertEquals(
                isSelectedAccount, model.get(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT));

        model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER).onResult(profileData);
        verify(mListenerMock).onAccountSelected(accountEmail, position == 0);
    }

    private void checkItemForAddAccountRow(int position) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.ADD_ACCOUNT_ROW, item.type);
        item.model.get(AddAccountRowProperties.ON_CLICK_LISTENER).run();
        verify(mListenerMock).addAccount();
    }

    private void addAccount(String accountName, String fullName) {
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(accountName, profileData);
    }
}
