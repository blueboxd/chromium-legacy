// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.accounts.Account;
import android.support.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.AccountManagerFacade;

/**
 * Structure storing the core information about a Google account that is always known. The {@link
 * CoreAccountInfo} for a given user is almost always the same but it may change in some rare cases.
 * For example, the {@link android.accounts.Account} will change if a user changes email.
 *
 * This class has a native counterpart called CoreAccountInfo. There are several differences between
 * these two classes:
 * - Android class doesn't store Gaia ID as a string because {@link CoreAccountId} already contains
 * it.
 * - Android class additionally exposes {@link android.accounts.Account} object for interactions
 * with the system.
 * - Android class has the "account name" whereas the native class has "email". This is the same
 * string, only the naming in different.
 */
public class CoreAccountInfo {
    private final CoreAccountId mId;
    private final Account mAccount;

    public CoreAccountInfo(@NonNull CoreAccountId id, @NonNull Account account) {
        assert id != null;
        assert account != null;

        mId = id;
        mAccount = account;
    }

    @CalledByNative
    private CoreAccountInfo(@NonNull String id, @NonNull String name) {
        assert id != null;
        assert name != null;
        mAccount = AccountManagerFacade.createAccountFromName(name);
        mId = new CoreAccountId(id);
    }

    /**
     * Returns a unique identifier of the current account.
     */
    public CoreAccountId getId() {
        return mId;
    }

    /**
     * Returns a name of the current account.
     */
    public String getName() {
        return mAccount.name;
    }

    /**
     * Returns {@link android.accounts.Account} object holding a name of the current account.
     */
    public Account getAccount() {
        return mAccount;
    }

    @Override
    public String toString() {
        return String.format("CoreAccountInfo{id[%s], name[%s]}", getId(), getName());
    }

    @Override
    public int hashCode() {
        return 31 * mId.hashCode() + mAccount.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountInfo)) return false;
        CoreAccountInfo other = (CoreAccountInfo) obj;
        return mId.equals(other.mId) && mAccount.equals(other.mAccount);
    }
}
