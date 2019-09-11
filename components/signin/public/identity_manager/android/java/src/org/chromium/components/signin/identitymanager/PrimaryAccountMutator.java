// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * PrimaryAccountMutator is the interface to set and clear the primary account (see IdentityManager
 * for more information).
 */
public class PrimaryAccountMutator {
    private static final String TAG = "PrimaryAccountMuta";

    private final long mNativePrimaryAccountMutator;

    @CalledByNative
    private PrimaryAccountMutator(long nativePrimaryAccountMutator) {
        assert nativePrimaryAccountMutator != 0;
        mNativePrimaryAccountMutator = nativePrimaryAccountMutator;
    }

    /**
     * Marks the account with |account_id| as the primary account, and returns whether the operation
     * succeeded or not. To succeed, this requires that:
     *   - the account is known by the IdentityManager.
     *   - setting the primary account is allowed,
     *   - the account username is allowed by policy,
     *   - there is not already a primary account set.
     */
    public boolean setPrimaryAccount(CoreAccountId accountId) {
        return PrimaryAccountMutatorJni.get().setPrimaryAccount(
                mNativePrimaryAccountMutator, accountId);
    }

    /**
     * Clears the primary account, and returns whether the operation succeeded or not. Depending on
     * |action|, the other accounts known to the IdentityManager may be deleted.
     */
    public boolean clearPrimaryAccount(@ClearAccountsAction int action,
            @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric) {
        return PrimaryAccountMutatorJni.get().clearPrimaryAccount(
                mNativePrimaryAccountMutator, action, sourceMetric, deleteMetric);
    }

    @NativeMethods
    interface Natives {
        public boolean setPrimaryAccount(long nativePrimaryAccountMutator, CoreAccountId accountId);
        public boolean clearPrimaryAccount(long nativePrimaryAccountMutator,
                @ClearAccountsAction int action, @SignoutReason int sourceMetric,
                @SignoutDelete int deleteMetric);
    }
}
