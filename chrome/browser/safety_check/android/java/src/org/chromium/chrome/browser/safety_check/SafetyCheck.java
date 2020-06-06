// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;

/**
 * Main class for all Safety check related logic.
 */
public class SafetyCheck implements SafetyCheckCommonObserver {
    private SafetyCheckBridge mSafetyCheckBridge;

    public SafetyCheck() {
        mSafetyCheckBridge = new SafetyCheckBridge(SafetyCheck.this);
    }

    /**
     * Triggers a Safe Browsing status check.
     */
    public void checkSafeBrowsing() {
        mSafetyCheckBridge.checkSafeBrowsing();
    }

    /**
     * Gets invoked once the Safe Browsing check is completed.
     * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing
     *         state (see //components/safety_check/safety_check.h).
     */
    @Override
    public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {}
}
