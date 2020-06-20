// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;

/**
 * Provides access to the C++ multi-platform Safety check code in
 * //components/safety_check.
 */
public class SafetyCheckBridge {
    /**
     * Observer for SafetyCheck code common to Desktop, Android, and iOS.
     */
    public interface SafetyCheckCommonObserver {
        /**
         * Gets invoked by the C++ code once the Safe Browsing check has results.
         *
         * @param status SafetyCheck::SafeBrowsingStatus enum value representing the
         *               Safe Browsing state (see
         *               //components/safety_check/safety_check.h).
         */
        @CalledByNative("SafetyCheckCommonObserver")
        void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status);

        /**
         * Gets invoked by the C++ code every time another credential is checked.
         *
         * @param checked Number of passwords already checked.
         * @param total   Total number of passwords to check.
         */
        @CalledByNative("SafetyCheckCommonObserver")
        void onPasswordCheckCredentialDone(int checked, int total);

        /**
         * Gets invoked by the C++ code when the status of the password check changes.
         *
         * @param state BulkLeakCheckService::State enum value representing the state
         *              (see //components/password_manager/core/browser
         *                  /bulk_leak_check_service_interface.h).
         */
        @CalledByNative("SafetyCheckCommonObserver")
        void onPasswordCheckStateChange(@BulkLeakCheckServiceState int state);
    }

    /**
     * Holds the C++ side of the Bridge class.
     */
    private long mNativeSafetyCheckBridge;

    /**
     * Initializes the C++ side.
     *
     * @param safetyCheckCommonObserver An observer instance that will receive the
     *                                  result of the check.
     */
    public SafetyCheckBridge(SafetyCheckCommonObserver safetyCheckCommonObserver) {
        mNativeSafetyCheckBridge =
                SafetyCheckBridgeJni.get().init(SafetyCheckBridge.this, safetyCheckCommonObserver);
    }

    /**
     * Triggers the Safe Browsing check on the C++ side.
     */
    void checkSafeBrowsing() {
        SafetyCheckBridgeJni.get().checkSafeBrowsing(
                mNativeSafetyCheckBridge, SafetyCheckBridge.this);
    }

    /**
     * Triggers the passwords check on the C++ side.
     */
    void checkPasswords() {
        SafetyCheckBridgeJni.get().checkPasswords(mNativeSafetyCheckBridge, SafetyCheckBridge.this);
    }

    /**
     * @return Number of password leaks discovered during the last check.
     */
    int getNumberOfPasswordLeaksFromLastCheck() {
        return SafetyCheckBridgeJni.get().getNumberOfPasswordLeaksFromLastCheck(
                mNativeSafetyCheckBridge, SafetyCheckBridge.this);
    }

    /**
     * @return Whether the user has some passwords saved.
     */
    boolean savedPasswordsExist() {
        return SafetyCheckBridgeJni.get().savedPasswordsExist(
                mNativeSafetyCheckBridge, SafetyCheckBridge.this);
    }

    /**
     * Stops observing the events of the passwords leak check.
     */
    void stopObservingPasswordsCheck() {
        SafetyCheckBridgeJni.get().stopObservingPasswordsCheck(
                mNativeSafetyCheckBridge, SafetyCheckBridge.this);
    }

    /**
     * Destroys the C++ side of the Bridge, freeing up all the associated memory.
     */
    void destroy() {
        assert mNativeSafetyCheckBridge != 0;
        SafetyCheckBridgeJni.get().destroy(mNativeSafetyCheckBridge, SafetyCheckBridge.this);
        mNativeSafetyCheckBridge = 0;
    }

    /**
     * C++ method signatures.
     */
    @NativeMethods
    interface Natives {
        long init(SafetyCheckBridge safetyCheckBridge, SafetyCheckCommonObserver observer);
        void checkSafeBrowsing(long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
        void checkPasswords(long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
        int getNumberOfPasswordLeaksFromLastCheck(
                long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
        boolean savedPasswordsExist(
                long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
        void stopObservingPasswordsCheck(
                long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
        void destroy(long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
    }
}
