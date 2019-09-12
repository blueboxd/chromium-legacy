// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * IdentityManager provides access to native IdentityManager's public API to java components.
 */
public class IdentityManager {
    private static final String TAG = "IdentityManager";

    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    public interface Observer {
        /**
         * Called when an account becomes the user's primary account.
         * This method is not called during a reauth.
         */
        void onPrimaryAccountSet(CoreAccountInfo account);

        /**
         * Called when the user moves from having a primary account to no longer having a primary
         * account (note that the user may still have an *unconsented* primary account after this
         * event).
         */
        void onPrimaryAccountCleared(CoreAccountInfo account);
    }

    private long mNativeIdentityManager;
    private IdentityMutator mIdentityMutator;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Called by native to create an instance of IdentityManager.
     * @param identityMutator can be null if native's IdentityManager received a null
     * IdentityMutator, this happens in tests.
     */
    @CalledByNative
    static private IdentityManager create(
            long nativeIdentityManager, @Nullable IdentityMutator identityMutator) {
        assert nativeIdentityManager != 0;
        return new IdentityManager(nativeIdentityManager, identityMutator);
    }

    @VisibleForTesting
    public IdentityManager(long nativeIdentityManager, IdentityMutator identityMutator) {
        mNativeIdentityManager = nativeIdentityManager;
        mIdentityMutator = identityMutator;
    }

    /**
     * Called by native upon KeyedService's shutdown
     */
    @CalledByNative
    private void destroy() {
        mNativeIdentityManager = 0;
    }

    /**
     * Registers a IdentityManager.Observer
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Unregisters a IdentityManager.Observer
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies observers that the primary account was set in C++.
     */
    @CalledByNative
    private void onPrimaryAccountSet(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountSet(account);
        }
    }

    /**
     * Notifies observers that the primary account was cleared in C++.
     */
    @CalledByNative
    @VisibleForTesting
    public void onPrimaryAccountCleared(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountCleared(account);
        }
    }

    /**
     * Returns whether the user's primary account is available.
     */
    public boolean hasPrimaryAccount() {
        return IdentityManagerJni.get().hasPrimaryAccount(mNativeIdentityManager);
    }

    /**
     * Looks up and returns information for account with given |email_address|. If the account
     * cannot be found, return a null value.
     */
    public @Nullable CoreAccountInfo
    findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(String email) {
        return IdentityManagerJni.get()
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        mNativeIdentityManager, email);
    }

    /*
     * Returns pointer to the object used to change the signed-in state of the
     * primary account.
     */
    public IdentityMutator getIdentityMutator() {
        return mIdentityMutator;
    }

    @NativeMethods
    interface Natives {
        public boolean hasPrimaryAccount(long nativeIdentityManager);
        public @Nullable CoreAccountInfo
        findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                long nativeIdentityManager, String email);
    }
}
