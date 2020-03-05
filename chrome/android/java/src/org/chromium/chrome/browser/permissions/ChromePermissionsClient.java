// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.webapps.WebApkActivity;
import org.chromium.components.permissions.PermissionsClient;

/** Chrome specific implementation of PermissionsClient. */
public class ChromePermissionsClient extends PermissionsClient {
    // Static holder to ensure safe initialization of the singleton instance.
    private static class Holder {
        private static final ChromePermissionsClient sInstance = new ChromePermissionsClient();
    }

    @Override
    public void onPermissionDenied(Activity activity, String[] deniedPermissions) {
        if (activity instanceof WebApkActivity && deniedPermissions.length > 0) {
            WebApkUma.recordAndroidRuntimePermissionDeniedInWebApk(deniedPermissions);
        }
    }

    @Override
    public void onPermissionRequested(Activity activity, String[] permissions) {
        if (activity instanceof WebApkActivity) {
            WebApkUma.recordAndroidRuntimePermissionPromptInWebApk(permissions);
        }
    }

    @CalledByNative
    public static ChromePermissionsClient get() {
        return Holder.sInstance;
    }
}
