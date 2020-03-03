// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.security_state.SecurityStateModelDelegate;

/**
 * Creates the C++ class that acts as the SecurityStateModelDelegate for Chrome.
 */
public class ChromeSecurityStateModelDelegate {
    private static SecurityStateModelDelegate sInstance = new SecurityStateModelDelegate(
            ChromeSecurityStateModelDelegateJni.get().createSecurityStateModelDelegate());

    /**
     * Get an instance of SecurityStateModelDelegate.
     */
    public static SecurityStateModelDelegate getInstance() {
        return sInstance;
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        long createSecurityStateModelDelegate();
    }
}
