// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * This bridge creates and initializes a {@link AllPasswordsBottomSheetCoordinator} on construction
 * and forwards native calls to it.
 */
class AllPasswordsBottomSheetBridge {
    private long mNativeView;
    private Credential[] mCredentials;

    private AllPasswordsBottomSheetBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        assert (mNativeView != 0);
    }

    @CalledByNative
    private static AllPasswordsBottomSheetBridge create(
            long nativeView, WindowAndroid windowAndroid) {
        return new AllPasswordsBottomSheetBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private void createCredentialArray(int size) {
        mCredentials = new Credential[size];
    }

    @CalledByNative
    private void insertCredential(int index, String username, String password,
            String formattedUsername, String originUrl, boolean isPublicSuffixMatch,
            boolean isAffiliationBasedMatch) {
        mCredentials[index] = new Credential(username, password, formattedUsername, originUrl,
                isPublicSuffixMatch, isAffiliationBasedMatch);
    }

    @CalledByNative
    private void showCredentials() {
        // TODO(crbug.com/1104132): Implement.
        // Temporary call to deleted native objects and avoid memory leak.
        AllPasswordsBottomSheetBridgeJni.get().onDismiss(mNativeView);
    }

    @NativeMethods
    interface Natives {
        void onCredentialSelected(
                long nativeAllPasswordsBottomSheetViewImpl, Credential credential);
        void onDismiss(long nativeAllPasswordsBottomSheetViewImpl);
    }
}