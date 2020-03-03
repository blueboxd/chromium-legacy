// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.components.signin.ChildAccountStatus;

/**
 * A helper for child account checks.
 * Usage:
 * new AndroidChildAccountHelper() { override onParametersReady() }.start(appContext).
 */
public abstract class AndroidChildAccountHelper implements Callback<Integer> {
    private @ChildAccountStatus.Status Integer mChildAccountStatus;

    /** The callback called when child account parameters are known. */
    public abstract void onParametersReady();

    /** @return The status of the device regarding child accounts. */
    protected @ChildAccountStatus.Status int getChildAccountStatus() {
        return mChildAccountStatus;
    }

    /**
     * Starts fetching the child accounts information.
     * Calls onParametersReady() once the information is fetched.
     */
    public void start() {
        ChildAccountService.checkChildAccountStatus(this);
    }

    // Callback<Integer>:
    @Override
    public void onResult(@ChildAccountStatus.Status Integer status) {
        mChildAccountStatus = status;
        if (mChildAccountStatus != null) {
            onParametersReady();
        }
    }
}
