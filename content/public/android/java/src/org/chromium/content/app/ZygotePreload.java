// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.pm.ApplicationInfo;
import android.os.Build;

import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;

/**
 * Class used in android:zygotePreloadName attribute of manifest.
 * Code in this class runs in the zygote. It runs in a limited environment
 * (eg no application) and cannot communicate with any other app process,
 * so care must be taken when writing code in this class. Eg it should not
 * create any thread.
 */
@TargetApi(Build.VERSION_CODES.Q)
public class ZygotePreload implements android.app.ZygotePreload {
    private static final String TAG = "ZygotePreload";

    @SuppressLint("Override")
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        try {
            JNIUtils.enableSelectiveJniRegistration();
            LibraryLoader.getInstance().loadNowInZygote(appInfo);
        } catch (Throwable e) {
            // Ignore any exception. Child service can continue loading.
            Log.w(TAG, "Exception in zygote", e);
        }
    }
}
