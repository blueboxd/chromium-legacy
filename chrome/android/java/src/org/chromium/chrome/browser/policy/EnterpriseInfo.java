// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;

import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.RejectedExecutionException;

// TODO: This class needs tests. https://crbug.com/1099271

/**
 * Provide the enterprise information for the current device and profile.
 */
public final class EnterpriseInfo {
    private static final String TAG = "EnterpriseInfo";

    // Only ever read/written on the UI thread.
    private static OwnedState sOwnedState = null;
    private static Queue<Callback<OwnedState>> sCallbackList =
            new LinkedList<Callback<OwnedState>>();

    private static class OwnedState {
        boolean mDeviceOwned;
        boolean mProfileOwned;

        public OwnedState(boolean isDeviceOwned, boolean isProfileOwned) {
            mDeviceOwned = isDeviceOwned;
            mProfileOwned = isProfileOwned;
        }
    }

    /**
     * Returns, via callback, whether the device has a device owner or a profile owner for native.
     */
    @CalledByNative
    public static void getManagedStateForNative() {
        Callback<OwnedState> callback = (result) -> {
            if (result == null) {
                // Unable to determine the owned state, assume it's not owned.
                EnterpriseInfoJni.get().updateNativeOwnedState(false, false);
            }

            EnterpriseInfoJni.get().updateNativeOwnedState(
                    result.mDeviceOwned, result.mProfileOwned);
        };

        getDeviceEnterpriseInfo(callback);
    }

    /**
     * Records metrics regarding whether the device has a device owner or a profile owner.
     */
    public static void logDeviceEnterpriseInfo() {
        Callback<OwnedState> callback = (result) -> {
            recordManagementHistograms(result);
        };

        getDeviceEnterpriseInfo(callback);
    }

    private static void getDeviceEnterpriseInfo(Callback<OwnedState> callback) {
        // AsyncTask requires being called from UI thread.
        ThreadUtils.assertOnUiThread();
        assert callback != null;

        if (sOwnedState != null) {
            callback.onResult(sOwnedState);
            return;
        }

        sCallbackList.add(callback);

        if (sCallbackList.size() > 1) {
            // A pending callback is already being worked on, no need to start up a new thread.
            return;
        }

        // This is the first request, spin up a thread.
        try {
            new AsyncTask<OwnedState>() {
                // TODO: Unit test this function. https://crbug.com/1099262
                private OwnedState calculateIsRunningOnManagedProfile(Context context) {
                    long startTime = SystemClock.elapsedRealtime();
                    boolean hasProfileOwnerApp = false;
                    boolean hasDeviceOwnerApp = false;
                    PackageManager packageManager = context.getPackageManager();
                    DevicePolicyManager devicePolicyManager =
                            (DevicePolicyManager) context.getSystemService(
                                    Context.DEVICE_POLICY_SERVICE);

                    for (PackageInfo pkg : packageManager.getInstalledPackages(/* flags= */ 0)) {
                        assert devicePolicyManager != null;
                        if (devicePolicyManager.isProfileOwnerApp(pkg.packageName)) {
                            hasProfileOwnerApp = true;
                        }
                        if (devicePolicyManager.isDeviceOwnerApp(pkg.packageName)) {
                            hasDeviceOwnerApp = true;
                        }
                        if (hasProfileOwnerApp && hasDeviceOwnerApp) break;
                    }

                    long endTime = SystemClock.elapsedRealtime();
                    RecordHistogram.recordTimesHistogram(
                            "EnterpriseCheck.IsRunningOnManagedProfileDuration",
                            endTime - startTime);

                    return new OwnedState(hasDeviceOwnerApp, hasProfileOwnerApp);
                }

                @Override
                protected OwnedState doInBackground() {
                    Context context = ContextUtils.getApplicationContext();
                    return calculateIsRunningOnManagedProfile(context);
                }

                @Override
                protected void onPostExecute(OwnedState result) {
                    // This is run on the UI thread.
                    assert result != null;

                    sOwnedState = result;
                    // Notify every waiting callback.
                    while (sCallbackList.size() > 0) {
                        sCallbackList.remove().onResult(sOwnedState);
                    }
                }
            }.executeWithTaskTraits(TaskTraits.USER_VISIBLE);
        } catch (RejectedExecutionException e) {
            // This is an extreme edge case, but if it does happen then return null to indicate we
            // couldn't execute.
            Log.w(TAG, "Thread limit reached, unable to determine managed state.");

            // There will only ever be a single item in the queue as we only try()/catch() on the
            // first item.
            sCallbackList.remove().onResult(null);
        }
    }

    private static void recordManagementHistograms(OwnedState state) {
        if (state == null) return;

        RecordHistogram.recordBooleanHistogram("EnterpriseCheck.IsManaged2", state.mProfileOwned);
        RecordHistogram.recordBooleanHistogram(
                "EnterpriseCheck.IsFullyManaged2", state.mDeviceOwned);
    }

    @NativeMethods
    interface Natives {
        void updateNativeOwnedState(boolean hasProfileOwnerApp, boolean hasDeviceOwnerApp);
    }
}
