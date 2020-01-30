// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.metrics;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.GmsBridge;

/**
 * Determines user consent and app opt-out for metrics. See metrics_service_client.h for more
 * explanation.
 *
 * TODO(weblayer-team): Consider compoentizing once requirements are nailed down.
 */
@JNINamespace("weblayer")
public class MetricsServiceClient {
    private static final String TAG = "MetricsServiceClie-";

    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics.
    private static final String AUTO_UPLOAD_METADATA_STR = "android.WebLayer.MetricsAutoUpload";

    private static boolean isAppOptedOut(Context ctx) {
        try {
            ApplicationInfo info = ctx.getPackageManager().getApplicationInfo(
                    ctx.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found which we interpret as not opting out.
                return false;
            }
            // getBoolean returns false if the key is not found, which is what we want.
            return info.metaData.getBoolean(AUTO_UPLOAD_METADATA_STR);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            // The conservative thing is to assume the app HAS opted out.
            return true;
        }
    }

    public static void init() {
        GmsBridge.getInstance().queryMetricsSetting(userConsent -> {
            MetricsServiceClientJni.get().setHaveMetricsConsent(
                    userConsent, !isAppOptedOut(ContextUtils.getApplicationContext()));
        });
    }

    @CalledByNative
    private static String getAppPackageName() {
        // Return this unconditionally; let native code enforce whether or not it's OK to include
        // this in the logs.
        Context ctx = ContextUtils.getApplicationContext();
        return ctx.getPackageName();
    }

    @CalledByNative
    private static boolean canRecordPackageNameForAppType() {
        // Only record if it's a system app or it was installed from Play Store.
        Context ctx = ContextUtils.getApplicationContext();
        String packageName = ctx.getPackageName();
        String installerPackageName = ctx.getPackageManager().getInstallerPackageName(packageName);
        return (ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0
                || (PLAY_STORE_PACKAGE_NAME.equals(installerPackageName));
    }

    @NativeMethods
    interface Natives {
        void setHaveMetricsConsent(boolean userConsent, boolean appConsent);
    }
}
