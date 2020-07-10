// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.List;

/**
 * Mediator of the account picker bottom sheet in web sign-in flow.
 */
class AccountPickerBottomSheetMediator implements AccountPickerCoordinator.Listener {
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final ProfileDataCache mProfileDataCache;
    private final PropertyModel mModel;

    private final ProfileDataCache.Observer mProfileDataSourceObserver =
            this::updateSelectedAccountData;
    private final AccountManagerFacade mAccountManagerFacade;
    private final AccountsChangeObserver mAccountsChangeObserver = this::onAccountListUpdated;
    private @Nullable String mSelectedAccountName = null;

    AccountPickerBottomSheetMediator(Context context, AccountPickerDelegate accountPickerDelegate) {
        mAccountPickerDelegate = accountPickerDelegate;
        mProfileDataCache = new ProfileDataCache(
                context, context.getResources().getDimensionPixelSize(R.dimen.user_picture_size));

        mModel = AccountPickerBottomSheetProperties.createModel(
                this::onSelectedAccountClicked, this::onContinueAsClicked);
        mProfileDataCache.addObserver(mProfileDataSourceObserver);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(mAccountsChangeObserver);
        onAccountListUpdated();
    }

    /**
     * Notifies that the user has selected an account.
     *
     * @param accountName The email of the selected account.
     * @param isDefaultAccount Whether the selected account is the first in the account list.
     */
    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        // Click on one account in the account list when the account list is expanded
        // will collapse it to the selected account
        mModel.set(AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED, false);
        setSelectedAccountName(accountName);
    }

    /**
     * Notifies when the user clicked the "add account" button.
     */
    @Override
    public void addAccount() {
        mAccountPickerDelegate.addAccount();
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        mProfileDataCache.removeObserver(mProfileDataSourceObserver);
        mAccountManagerFacade.removeObserver(mAccountsChangeObserver);
    }

    /**
     * Updates the collapsed account list when account list changes.
     *
     * Implements {@link AccountsChangeObserver}.
     */
    private void onAccountListUpdated() {
        List<Account> accounts = mAccountManagerFacade.tryGetGoogleAccounts();
        if (accounts.isEmpty()) {
            // If all accounts disappeared when the account list is collapsed, we will
            // go to the zero account screen. If the account list is expanded, we will
            // first set the account list state to collapsed then move to the zero
            // account collapsed screen.
            mModel.set(AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED, false);
            mSelectedAccountName = null;
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA, null);
        } else {
            // TODO(https://crbug.com/1096977): Test and handle the scenario when
            // the existing account list changes to a different list, we need to
            // separate the two cases when the current selected account name is in the
            // new list and not in the new list.
            setSelectedAccountName(accounts.get(0).name);
        }
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        mProfileDataCache.update(Collections.singletonList(mSelectedAccountName));
        updateSelectedAccountData(mSelectedAccountName);
    }

    /**
     * Implements {@link ProfileDataCache.Observer}.
     */
    private void updateSelectedAccountData(String accountName) {
        if (TextUtils.equals(mSelectedAccountName, accountName)) {
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountName));
        }
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        // Clicking on the selected account when the account list is collapsed will expand the
        // account list and make the account list visible
        mModel.set(AccountPickerBottomSheetProperties.IS_ACCOUNT_LIST_EXPANDED, true);
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        if (mSelectedAccountName == null) {
            addAccount();
        } else {
            mAccountPickerDelegate.signIn(mSelectedAccountName);
        }
    }
}
