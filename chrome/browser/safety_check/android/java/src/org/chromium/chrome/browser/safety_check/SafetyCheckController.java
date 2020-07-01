// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;

/** Main class for all Safety check related logic. */
public class SafetyCheckController implements SafetyCheckCommonObserver {
    private SafetyCheckBridge mSafetyCheckBridge;
    private SafetyCheckModel mModel;
    private SafetyCheckUpdatesDelegate mUpdatesClient;

    /**
     * Creates a new instance of the Safety check controller given a model.
     *
     * @param model A model instance.
     */
    public SafetyCheckController(SafetyCheckModel model, SafetyCheckUpdatesDelegate client) {
        mSafetyCheckBridge = new SafetyCheckBridge(SafetyCheckController.this);
        mModel = model;
        mUpdatesClient = client;
    }

    @VisibleForTesting
    SafetyCheckController(
            SafetyCheckModel model, SafetyCheckUpdatesDelegate client, SafetyCheckBridge bridge) {
        mModel = model;
        mUpdatesClient = client;
        mSafetyCheckBridge = bridge;
    }

    /** Triggers all safety check child checks. */
    public void performSafetyCheck() {
        mModel.setCheckingState();
        mSafetyCheckBridge.checkSafeBrowsing();
        mSafetyCheckBridge.checkPasswords();
        mUpdatesClient.checkForUpdates((status) -> { mModel.updateUpdatesStatus(status); });
    }

    /**
     * Gets invoked once the Safe Browsing check is completed.
     *
     * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state
     *     (see //components/safety_check/safety_check.h).
     */
    @Override
    public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {
        mModel.updateSafeBrowsingStatus(status);
    }

    /**
     * Gets invoked by the C++ code every time another credential is checked.
     *
     * @param checked Number of passwords already checked.
     * @param total Total number of passwords to check.
     */
    @Override
    public void onPasswordCheckCredentialDone(int checked, int total) {}

    /**
     * Gets invoked by the C++ code when the status of the password check changes.
     *
     * @param state BulkLeakCheckService::State enum value representing the state (see
     *     //components/password_manager/core/browser/bulk_leak_check_service_interface.h).
     */
    @Override
    public void onPasswordCheckStateChange(@BulkLeakCheckServiceState int state) {
        if (state == BulkLeakCheckServiceState.RUNNING) {
            return;
        }
        mSafetyCheckBridge.stopObservingPasswordsCheck();
        if (state == BulkLeakCheckServiceState.IDLE) {
            boolean hasPasswords = mSafetyCheckBridge.savedPasswordsExist();
            int numLeaked = mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck();
            mModel.updatePasswordsStatusOnSucess(hasPasswords, numLeaked);
        } else {
            mModel.updatePasswordsStatusOnError(state);
        }
    }
}
