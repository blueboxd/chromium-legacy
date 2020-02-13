// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.concurrent.RejectedExecutionException;

/**
 * Provide the enterprise information for the current device.
 * Currently only contains one function to record the histogram for "EnterpriseCheck.IsManaged".
 */
public final class EnterpriseInfo {
    public static void logDeviceEnterpriseInfo() {
        assert BrowserStartupController.getInstance().isFullBrowserStarted();

        try {
            new AsyncTask<Boolean>() {
                private Boolean calculateIsRunningOnManagedProfile(Context context) {
                    if (VERSION.SDK_INT < VERSION_CODES.M) return null;

                    PackageManager packageManager = context.getPackageManager();
                    DevicePolicyManager devicePolicyManager =
                            context.getSystemService(DevicePolicyManager.class);

                    for (PackageInfo pkg : packageManager.getInstalledPackages(/* flags= */ 0)) {
                        assert devicePolicyManager != null;
                        if (devicePolicyManager.isProfileOwnerApp(pkg.packageName)) {
                            return true;
                        }
                    }

                    return false;
                }

                @Override
                protected Boolean doInBackground() {
                    Context context = ContextUtils.getApplicationContext();

                    return calculateIsRunningOnManagedProfile(context);
                }

                @Override
                protected void onPostExecute(Boolean isManagedDevice) {
                    if (isManagedDevice == null) return;

                    RecordHistogram.recordBooleanHistogram(
                            "EnterpriseCheck.IsManaged", isManagedDevice);
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } catch (RejectedExecutionException e) {
            // Fail silently here since this is not a critical task.
        }
    }
}
