// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.dev_ui;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.UsedByReflection;

/** Helpers for DevUI DFM installation. */
@JNINamespace("dev_ui")
@UsedByReflection("DevUiModuleProvider")
public class DevUiModuleProvider {
    private static final String TAG = "DEV_UI";

    private long mNativeDevUiModuleProvider;

    @CalledByNative
    private DevUiModuleProvider(long nativeDevUiModuleProvider) {
        mNativeDevUiModuleProvider = nativeDevUiModuleProvider;
    }

    @CalledByNative
    private static boolean isModuleInstalled() {
        return DevUiModule.isInstalled();
    }

    @CalledByNative
    private void installModule() {
        DevUiModule.install((success) -> {
            Log.i(TAG, "Install status: %s", success);
            if (mNativeDevUiModuleProvider != 0) {
                nativeOnInstallResult(mNativeDevUiModuleProvider, success);
            }
        });
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeDevUiModuleProvider = 0;
    }

    private native void nativeOnInstallResult(long nativeDevUiModuleProvider, boolean success);
}
