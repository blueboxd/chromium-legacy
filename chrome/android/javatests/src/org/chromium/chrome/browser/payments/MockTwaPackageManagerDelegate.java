// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;

/** Simulates a TWA package manager in memory. */
class MockTwaPackageManagerDelegate extends TwaPackageManagerDelegate {
    private String mMockTwaPackage;

    /**
     * Mock the current package to be a Trust Web Activity package.
     * @param mockTwaPackage The intended package nam, not allowed to be null.
     */
    public void setMockTrustedWebActivity(String mockTwaPackage) {
        assert mockTwaPackage != null;
        mMockTwaPackage = mockTwaPackage;
    }

    @Override
    @Nullable
    public String getTwaPackageName(ChromeActivity activity) {
        return mMockTwaPackage != null ? mMockTwaPackage : super.getTwaPackageName(activity);
    }
}
