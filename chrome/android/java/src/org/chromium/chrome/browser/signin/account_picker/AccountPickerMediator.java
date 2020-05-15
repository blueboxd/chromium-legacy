// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ItemType;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The mediator of account picker handles all the signals from the outside world.
 *
 * It defines the business logic when the user selects or adds an account and updates the model.
 * This class has no visibility of the account picker view.
 */
class AccountPickerMediator {
    private final MVCListAdapter.ModelList mListModel;
    private final AccountPickerCoordinator.Listener mAccountPickerListener;
    private final ProfileDataCache mProfileDataCache;
    private @Nullable String mSelectedAccountName;

    private final AccountManagerFacade mAccountManagerFacade;
    private final AccountsChangeObserver mAccountsChangeObserver = this::updateAccounts;
    private final ProfileDataCache.Observer mProfileDataObserver = accountId -> updateProfileData();
    private List<String> mAccountNames;

    AccountPickerMediator(Context context, MVCListAdapter.ModelList listModel,
            AccountPickerCoordinator.Listener listener, @Nullable String selectedAccountName) {
        mListModel = listModel;
        mAccountPickerListener = listener;
        mProfileDataCache = new ProfileDataCache(
                context, context.getResources().getDimensionPixelSize(R.dimen.user_picture_size));
        mSelectedAccountName = selectedAccountName;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        mAccountManagerFacade.addObserver(mAccountsChangeObserver);
        mProfileDataCache.addObserver(mProfileDataObserver);
        updateAccounts();
    }

    /**
     * Unregisters the observers used by the mediator.
     */
    void destroy() {
        mProfileDataCache.removeObserver(mProfileDataObserver);
        mAccountManagerFacade.removeObserver(mAccountsChangeObserver);
    }

    void setSelectedAccountName(String selectedAccountName) {
        if (!TextUtils.equals(mSelectedAccountName, selectedAccountName)) {
            mSelectedAccountName = selectedAccountName;
            updateSelectedAccount();
        }
    }

    private void updateAccounts() {
        mAccountNames = AccountUtils.toAccountNames(mAccountManagerFacade.tryGetGoogleAccounts());
        mProfileDataCache.update(mAccountNames);
        updateProfileData();
    }

    private void updateProfileData() {
        mListModel.clear();
        // Add an "existing account" row for each account
        if (mAccountNames.size() > 0) {
            DisplayableProfileData profileData =
                    mProfileDataCache.getProfileDataOrDefault(mAccountNames.get(0));
            mListModel.add(createExistingAccountRowItem(profileData, true));
            for (int i = 1; i < mAccountNames.size(); ++i) {
                profileData = mProfileDataCache.getProfileDataOrDefault(mAccountNames.get(i));
                mListModel.add(createExistingAccountRowItem(profileData, false));
            }
        }
        // Add an "add account" row
        PropertyModel model =
                AddAccountRowProperties.createModel(mAccountPickerListener::addAccount);
        mListModel.add(new MVCListAdapter.ListItem(ItemType.ADD_ACCOUNT_ROW, model));
    }

    private MVCListAdapter.ListItem createExistingAccountRowItem(
            DisplayableProfileData profileData, boolean isDefaultAccount) {
        Callback<DisplayableProfileData> profileDataCallback = profileData1
                -> mAccountPickerListener.onAccountSelected(
                        profileData1.getAccountName(), isDefaultAccount);
        PropertyModel model = ExistingAccountRowProperties.createModel(profileData,
                profileDataCallback, profileData.getAccountName().equals(mSelectedAccountName));
        return new MVCListAdapter.ListItem(ItemType.EXISTING_ACCOUNT_ROW, model);
    }

    private void updateSelectedAccount() {
        for (int i = 0; i < mListModel.size() - 1; ++i) {
            PropertyModel model = mListModel.get(i).model;
            boolean isSelectedAccount = TextUtils.equals(mSelectedAccountName,
                    model.get(ExistingAccountRowProperties.PROFILE_DATA).getAccountName());
            model.set(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT, isSelectedAccount);
        }
    }
}
