// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

class SafetyCheckMediator implements SafetyCheckCommonObserver {
    /** Bridge to the C++ side for the Safe Browsing and passwords checks. */
    private SafetyCheckBridge mSafetyCheckBridge;
    /** Model representing the current state of the checks. */
    private PropertyModel mModel;
    /** Client to interact with Omaha for the updates check. */
    private SafetyCheckUpdatesDelegate mUpdatesClient;

    private final SharedPreferencesManager mPreferenceManager;

    /**
     * Callback that gets invoked once the result of the updates check is available. Not inlined
     * because a {@link WeakReference} needs to be passed (the check is asynchronous).
     */
    private final Callback<Integer> mUpdatesCheckCallback = (status) -> {
        if (mModel != null) {
            mModel.set(SafetyCheckProperties.UPDATES_STATE, status);
        }
    };

    /**
     * Creates a new instance of the Safety check mediator given a model and an updates client.
     *
     * @param model A model instance.
     * @param client An updates client.
     */
    public SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client) {
        this(model, client, null);
        // Have to initialize this after the constructor call, since a "this" instance is needed.
        mSafetyCheckBridge = new SafetyCheckBridge(SafetyCheckMediator.this);
    }

    @VisibleForTesting
    SafetyCheckMediator(
            PropertyModel model, SafetyCheckUpdatesDelegate client, SafetyCheckBridge bridge) {
        mModel = model;
        mUpdatesClient = client;
        mSafetyCheckBridge = bridge;
        mPreferenceManager = SharedPreferencesManager.getInstance();
        // Set the listener for clicking the Check button.
        mModel.set(SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER,
                (View.OnClickListener) (v) -> performSafetyCheck());
    }

    /** Triggers all safety check child checks. */
    public void performSafetyCheck() {
        // Increment the stored number of Safety check starts.
        mPreferenceManager.incrementInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER);
        // Set the checking state for all elements.
        mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.CHECKING);
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
        mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
        // Start all the checks.
        mSafetyCheckBridge.checkSafeBrowsing();
        mSafetyCheckBridge.checkPasswords();
        mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
    }

    /**
     * Gets invoked once the Safe Browsing check is completed.
     *
     * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state
     *     (see //components/safety_check/safety_check.h).
     */
    @Override
    public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE,
                SafetyCheckProperties.safeBrowsingStateFromNative(status));
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
        // Handle the error states.
        if (state != BulkLeakCheckServiceState.IDLE) {
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE,
                    SafetyCheckProperties.passwordsStatefromErrorState(state));
            return;
        }
        // Non-error state depends on whether there are any passwords saved and/or leaked.
        if (!mSafetyCheckBridge.savedPasswordsExist()) {
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.NO_PASSWORDS);
        } else if (mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck() == 0) {
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SAFE);
        } else {
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.COMPROMISED_EXIST);
        }
    }

    /** Cancels any pending callbacks and registered observers.  */
    public void destroy() {
        mSafetyCheckBridge.destroy();
        mSafetyCheckBridge = null;
        mUpdatesClient = null;
        mModel = null;
    }
}
