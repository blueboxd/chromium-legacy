// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides access to the C++ multi-platform Safety check code in //components/safety_check.
 */
public class SafetyCheckBridge {
    /**
     * Observer for SafetyCheck code common to Desktop, Android, and iOS.
     */
    public interface SafetyCheckCommonObserver {
        /**
         * Gets invoked by the C++ code once the Safe Browsing check has results.
         * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing
         *         state (see //components/safety_check/safety_check.h).
         */
        @CalledByNative("SafetyCheckCommonObserver")
        void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status);
    }

    /**
     * Holds the C++ side of the Bridge class.
     */
    private long mNativeSafetyCheckBridge;

    /**
     * Initializes the C++ side.
     * @param safetyCheckCommonObserver an observer instance that will receive the result of the
     *  check using
     *  {@link SafetyCheckCommonObserver#onSafeBrowsingCheckResult(SafeBrowsingStatus)}.
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
        void destroy(long nativeSafetyCheckBridge, SafetyCheckBridge safetyCheckBridge);
    }
}
