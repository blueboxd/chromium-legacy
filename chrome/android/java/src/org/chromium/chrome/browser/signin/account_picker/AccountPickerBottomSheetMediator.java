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
import org.chromium.components.signin.AccountManagerFacadeProvider;
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
    private @Nullable String mSelectedAccountName = null;

    AccountPickerBottomSheetMediator(Context context, AccountPickerDelegate accountPickerDelegate) {
        mAccountPickerDelegate = accountPickerDelegate;
        mProfileDataCache = new ProfileDataCache(
                context, context.getResources().getDimensionPixelSize(R.dimen.user_picture_size));

        mModel = AccountPickerBottomSheetProperties.createModel(
                this::onSelectedAccountClicked, this::onContinueAsClicked);
        mProfileDataCache.addObserver(mProfileDataSourceObserver);
        // TODO(https://crbug.com/1096977): Add an observer to listen to accounts
        // change in AccountManagerFacade, in case from zero to more accounts or
        // the selected account disappeared.
        List<Account> accounts = AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        if (!accounts.isEmpty()) {
            setSelectedAccountName(accounts.get(0).name);
        }
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
