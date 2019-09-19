// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;

/**
 * This bridge creates and initializes a {@link TouchToFillComponent} on construction and forwards
 * native calls to it.
 */
class TouchToFillBridge {
    private long mNativeView;
    private final TouchToFillComponent mTouchToFillComponent;

    private TouchToFillBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mTouchToFillComponent = new TouchToFillCoordinator();
        mTouchToFillComponent.initialize(
                activity, activity.getBottomSheetController(), this::onDismissed);
    }

    @CalledByNative
    private static TouchToFillBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new TouchToFillBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private static Credential[] createCredentialArray(int size) {
        return new Credential[size];
    }

    @CalledByNative
    private static void insertCredential(Credential[] credentials, int index, String username,
            String password, String originUrl, boolean isPublicSuffixMatch) {
        credentials[index] = new Credential(username, password, originUrl, isPublicSuffixMatch);
    }

    @CalledByNative
    private void showCredentials(String formattedUrl, Credential[] credentials) {
        mTouchToFillComponent.showCredentials(
                formattedUrl, Arrays.asList(credentials), this::onSelectCredential);
    }

    private void onDismissed() {
        // TODO(crbug.com/957532): Call native side to continue dismissing.
        mNativeView = 0; // The native view shouldn't be used after it's dismissed.
    }

    private void onSelectCredential(Credential credential) {
        assert mNativeView != 0 : "The native side is already dismissed";
        // TODO(crbug.com/957532): Call native side to continue filling.
    }
}
