// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import androidx.fragment.app.Fragment;

/**
 * Use {@link #createComponent()} to instantiate a {@link ManualFillingComponent}.
 */
public class CableAuthenticatorUIFactory {
    private CableAuthenticatorUIFactory() {}

    /**
     * Returns whether this feature is available in the current build.
     *
     * If false, no other methods of this class should be called.
     */
    public static boolean isAvailable() {
        return false;
    }

    /**
     * Returns the class of {@link CableAuthenticatorUI}.
     */
    public static Class<? extends Fragment> getFragmentClass() {
        return null;
    }
}
