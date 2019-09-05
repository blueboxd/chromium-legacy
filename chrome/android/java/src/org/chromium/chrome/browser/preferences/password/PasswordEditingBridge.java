// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;

/**
 * This is a bridge between PasswordEntryEditor and the C++ code. The bridge is in charge of
 * launching the PasswordEntryEditor and handling the password changes that happen through the
 * PasswordEntryEditor.
 */
public class PasswordEditingBridge {
    private long mNativePasswordEditingBridge;

    public PasswordEditingBridge(long nativePasswordEditingBridge) {
        mNativePasswordEditingBridge = nativePasswordEditingBridge;
    }

    @CalledByNative
    private static PasswordEditingBridge create(long nativePasswordEditingBridge) {
        return new PasswordEditingBridge(nativePasswordEditingBridge);
    }

    @CalledByNative
    private void showEditingUI(Context context, String site, String username, String password) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_URL, site);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_NAME, username);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_PASSWORD, password);
        PreferencesLauncher.launchSettingsPage(context, PasswordEntryEditor.class, fragmentArgs);
    }

    /**
     * Destroy the native object.
     */
    public void destroy() {
        if (mNativePasswordEditingBridge != 0) {
            nativeDestroy(mNativePasswordEditingBridge);
            mNativePasswordEditingBridge = 0;
        }
    }

    private native void nativeDestroy(long nativePasswordEditingBridge);
}
