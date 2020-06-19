// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.accounts.Account;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * This class is used in web sign-in flow for the account picker bottom sheet.
 *
 * It is responsible for the sign-in and adding account functions needed for the
 * web sign-in flow.
 */
public class AccountPickerDelegate implements AccountPickerCoordinator.Listener {
    private final ChromeActivity mChromeActivity;
    private final Tab mTab;

    public AccountPickerDelegate(ChromeActivity chromeActivity) {
        mChromeActivity = chromeActivity;
        // TODO(https://crbug.com/1095554): Check if website redirects after sign-in
        mTab = mChromeActivity.getActivityTab();
    }
    /**
     * Notifies that the user has selected an account.
     *
     * @param accountName The email of the selected account.
     * @param isDefaultAccount Whether the selected account is the first in the account list.
     */
    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        Account account = AccountUtils.findAccountByName(
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(), accountName);
        IdentityServicesProvider.get().getSigninManager().signIn(
                SigninAccessPoint.WEB_SIGNIN, account, new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // TODO(https://crbug.com/1092399): Implement sign-in properly in delegate
                        // We should wait for the cookie signin and cookie regeneration here,
                        // PostTask.postDelayedTask is just a temporary measure for testing the
                        // flow, it will be removed soon.
                        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
                            // TODO(https//crbug.com/1092399):
                            // Replace this with a continue URL got from GAIA
                            mTab.loadUrl(new LoadUrlParams("https://google.com"));
                        }, 2000);
                    }

                    @Override
                    public void onSignInAborted() {
                        // TODO(https//crbug.com/1092399): Add UI to show in this case
                    }
                });
    }

    /**
     * Notifies when the user clicked the "add account" button.
     */
    @Override
    public void addAccount() {
        // TODO(https//crbug.com/1097031): We should select the added account
        // and collapse the account chooser after the account is actually added.
        AccountManagerFacadeProvider.getInstance().createAddAccountIntent(
                (@Nullable Intent intent) -> {
                    if (intent != null) {
                        mChromeActivity.startActivity(intent);
                    } else {
                        // AccountManagerFacade couldn't create intent, use SigninUtils to open
                        // settings instead.
                        SigninUtils.openSettingsForAllAccounts(mChromeActivity);
                    }
                });
    }
}
