// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Determines user consent and app opt-out for metrics. See aw_metrics_service_client.h for more
 * explanation.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    private static final String TAG = "AwMetricsServiceCli-";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics
    // reporting. See https://developer.android.com/reference/android/webkit/WebView.html
    private static final String OPT_OUT_META_DATA_STR = "android.webkit.WebView.MetricsOptOut";

    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";

    private static boolean isAppOptedOut(Context ctx) {
        try {
            ApplicationInfo info = ctx.getPackageManager().getApplicationInfo(
                    ctx.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            // getBoolean returns false if the key is not found, which is what we want.
            return info.metaData.getBoolean(OPT_OUT_META_DATA_STR);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            // The conservative thing is to assume the app HAS opted out.
            return true;
        }
    }

    private static boolean shouldRecordPackageName(Context ctx) {
        // Only record if it's a system app or it was installed from Play Store.
        String packageName = ctx.getPackageName();
        String installerPackageName = ctx.getPackageManager().getInstallerPackageName(packageName);
        return (ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0
                || (PLAY_STORE_PACKAGE_NAME.equals(installerPackageName));
    }

    public static void setConsentSetting(Context ctx, boolean userConsent) {
        ThreadUtils.assertOnUiThread();
        AwMetricsServiceClientJni.get().setHaveMetricsConsent(userConsent, !isAppOptedOut(ctx));
    }

    @CalledByNative
    private static String getAppPackageName() {
        Context ctx = ContextUtils.getApplicationContext();
        return shouldRecordPackageName(ctx) ? ctx.getPackageName() : null;
    }

    @NativeMethods
    interface Natives {
        void setHaveMetricsConsent(boolean userConsent, boolean appConsent);
    }
}
