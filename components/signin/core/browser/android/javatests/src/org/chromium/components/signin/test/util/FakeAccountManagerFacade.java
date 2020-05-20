// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerResult;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ProfileDataSource;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * FakeAccountManagerFacade is an {@link AccountManagerFacade} stub intended
 * for testing.
 */
public class FakeAccountManagerFacade implements AccountManagerFacade {
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Set<AccountHolder> mAccountHolders = new LinkedHashSet<>();

    private final @Nullable FakeProfileDataSource mFakeProfileDataSource;

    /**
     * Creates an object of FakeAccountManagerFacade.
     * @param fakeProfileDataSource A FakeProfileDataSource instance if needed.
     */
    public FakeAccountManagerFacade(@Nullable FakeProfileDataSource fakeProfileDataSource) {
        mFakeProfileDataSource = fakeProfileDataSource;
    }

    @Override
    @Nullable
    public ProfileDataSource getProfileDataSource() {
        return mFakeProfileDataSource;
    }

    @Override
    public void waitForPendingUpdates(Runnable callback) {}

    @Override
    public void addObserver(AccountsChangeObserver observer) {}

    @Override
    public void removeObserver(AccountsChangeObserver observer) {}

    @Override
    public void runAfterCacheIsPopulated(Runnable runnable) {}

    @Override
    public boolean isCachePopulated() {
        return true;
    }

    @Override
    public List<Account> getGoogleAccounts() {
        List<Account> accounts = new ArrayList<>();
        synchronized (mLock) {
            for (AccountHolder accountHolder : mAccountHolders) {
                accounts.add(accountHolder.getAccount());
            }
        }
        return accounts;
    }

    @Override
    public void getGoogleAccounts(Callback<AccountManagerResult<List<Account>>> callback) {}

    @Override
    public boolean hasGoogleAccountAuthenticator() {
        return true;
    }

    @Override
    public String getAccessToken(Account account, String scope) {
        return "";
    }

    @Override
    public void invalidateAccessToken(String accessToken) {}

    @Override
    public void checkChildAccountStatus(Account account, Callback<Integer> callback) {}

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {}

    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {}

    @Override
    public String getAccountGaiaId(String accountEmail) {
        return "gaia-id-" + accountEmail.replace("@", "_at_");
    }

    @Override
    public boolean isGooglePlayServicesAvailable() {
        return true;
    }

    void addAccount(Account account) {
        AccountHolder accountHolder = AccountHolder.builder(account).alwaysAccept(true).build();
        // As this class is accessed both from UI thread and worker threads, we lock the access
        // to account holders to avoid potential race condition.
        synchronized (mLock) {
            mAccountHolders.add(accountHolder);
        }
    }

    void setProfileData(String accountId, @Nullable ProfileDataSource.ProfileData profileData) {
        assert mFakeProfileDataSource != null : "ProfileDataSource was disabled!";
        ThreadUtils.runOnUiThreadBlocking(
                () -> mFakeProfileDataSource.setProfileData(accountId, profileData));
    }
}
