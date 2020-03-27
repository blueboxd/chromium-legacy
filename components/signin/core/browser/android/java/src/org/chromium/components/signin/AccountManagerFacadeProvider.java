// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;

import java.util.concurrent.atomic.AtomicReference;

/**
 * AccountManagerFacadeProvider is intended to group all the
 * AccountManagerFacade instance manipulation methods in one place.
 */
public class AccountManagerFacadeProvider {
    private static final AtomicReference<AccountManagerFacade> sAtomicInstance =
            new AtomicReference<>();
    private static AccountManagerFacade sInstance;
    private static AccountManagerFacade sTestingInstance;

    private AccountManagerFacadeProvider() {}

    /**
     * Initializes AccountManagerFacade singleton instance. Can only be called once.
     * Tests can override the instance with {@link #overrideAccountManagerFacadeForTests}.
     *
     * @param delegate the AccountManagerDelegate to use
     */
    @MainThread
    public static void initializeAccountManagerFacade(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) {
            throw new IllegalStateException("AccountManagerFacade is already initialized!");
        }
        sInstance = new AccountManagerFacade(delegate);
        if (sTestingInstance != null) return;
        sAtomicInstance.set(sInstance);
    }

    /**
     * Overrides AccountManagerFacade singleton instance for tests. Only for use in Tests.
     * Overrides any previous or future calls to {@link #initializeAccountManagerFacade}.
     *
     * @param delegate the AccountManagerDelegate to use
     */
    @VisibleForTesting
    @AnyThread
    public static void overrideAccountManagerFacadeForTests(AccountManagerDelegate delegate) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            sTestingInstance = new AccountManagerFacade(delegate);
            sAtomicInstance.set(sTestingInstance);
        });
    }

    /**
     * Resets custom AccountManagerFacade set with {@link #overrideAccountManagerFacadeForTests}.
     * Only for use in Tests.
     */
    @VisibleForTesting
    @AnyThread
    public static void resetAccountManagerFacadeForTests() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            sTestingInstance = null;
            sAtomicInstance.set(sInstance);
        });
    }

    /**
     * Singleton instance getter. Singleton must be initialized before calling this by
     * {@link #initializeAccountManagerFacade} or {@link #overrideAccountManagerFacadeForTests}.
     *
     * @return a singleton instance
     */
    @AnyThread
    @CalledByNative
    public static AccountManagerFacade getInstance() {
        AccountManagerFacade instance = sAtomicInstance.get();
        assert instance != null : "AccountManagerFacade is not initialized!";
        return instance;
    }
}
