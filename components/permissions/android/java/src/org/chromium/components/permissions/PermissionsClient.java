// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;

/**
 * Class implemented by the embedder to access embedder specific logic.
 */
public class PermissionsClient {
    /**
     * Called when an Android permission request has been denied.
     * */
    public void onPermissionDenied(Activity activity, String[] deniedPermissions) {}

    /**
     * Called when Android permissions have been requested.
     * */
    public void onPermissionRequested(Activity activity, String[] permissions) {}

    /** Returns a default PermissionsClient. */
    @CalledByNative
    private static PermissionsClient get() {
        return new PermissionsClient();
    }
}
