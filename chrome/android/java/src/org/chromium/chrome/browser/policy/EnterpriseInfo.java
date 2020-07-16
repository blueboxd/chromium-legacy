// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

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

/**
 * Provide the enterprise information for the current device and profile.
 */
public final class EnterpriseInfo {
    private static final String TAG = "EnterpriseInfo";

    private static EnterpriseInfo sInstance;

    // Only ever read/written on the UI thread.
    private OwnedState mOwnedState;
    private Queue<Callback<OwnedState>> mCallbackList;

    private boolean mSkipAsyncCheckForTesting;

    static class OwnedState {
        boolean mDeviceOwned;
        boolean mProfileOwned;

        public OwnedState(boolean isDeviceOwned, boolean isProfileOwned) {
            mDeviceOwned = isDeviceOwned;
            mProfileOwned = isProfileOwned;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (!(other instanceof OwnedState)) return false;

            OwnedState otherOwnedState = (OwnedState) other;

            return this.mDeviceOwned == otherOwnedState.mDeviceOwned
                    && this.mProfileOwned == otherOwnedState.mProfileOwned;
        }
    }

    public static EnterpriseInfo getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) sInstance = new EnterpriseInfo();

        return sInstance;
    }

    /**
     * Returns, via callback, whether the device has a device owner or a profile owner.
     */
    public void getDeviceEnterpriseInfo(Callback<OwnedState> callback) {
        // AsyncTask requires being called from UI thread.
        ThreadUtils.assertOnUiThread();
        assert callback != null;

        if (mOwnedState != null) {
            callback.onResult(mOwnedState);
            return;
        }

        mCallbackList.add(callback);

        if (mCallbackList.size() > 1) {
            // A pending callback is already being worked on, no need to start up a new thread.
            return;
        }

        if (mSkipAsyncCheckForTesting) return;

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
                    onEnterpriseInfoResult(result);
                }
            }.executeWithTaskTraits(TaskTraits.USER_VISIBLE);
        } catch (RejectedExecutionException e) {
            // This is an extreme edge case, but if it does happen then return null to indicate we
            // couldn't execute.
            Log.w(TAG, "Thread limit reached, unable to determine managed state.");

            // There will only ever be a single item in the queue as we only try()/catch() on the
            // first item.
            mCallbackList.remove().onResult(null);
        }
    }

    /**
     * Records metrics regarding whether the device has a device owner or a profile owner.
     */
    public void logDeviceEnterpriseInfo() {
        Callback<OwnedState> callback = (result) -> {
            recordManagementHistograms(result);
        };

        getDeviceEnterpriseInfo(callback);
    }

    private EnterpriseInfo() {
        mOwnedState = null;
        mCallbackList = new LinkedList<Callback<OwnedState>>();
    }

    @VisibleForTesting
    void onEnterpriseInfoResult(OwnedState result) {
        ThreadUtils.assertOnUiThread();
        assert result != null;

        // Set the cached value.
        mOwnedState = result;

        // Service every waiting callback.
        while (mCallbackList.size() > 0) {
            // This implementation assumes that ever future call to getDeviceEnterpriseInfo(), from
            // this point, will result in the cached value being returned immediately. This means we
            // can ignore the issue of re-entrant callbacks.
            mCallbackList.remove().onResult(mOwnedState);
        }
    }

    private void recordManagementHistograms(OwnedState state) {
        if (state == null) return;

        RecordHistogram.recordBooleanHistogram("EnterpriseCheck.IsManaged2", state.mProfileOwned);
        RecordHistogram.recordBooleanHistogram(
                "EnterpriseCheck.IsFullyManaged2", state.mDeviceOwned);
    }

    @VisibleForTesting
    void setSkipAsyncCheckForTesting(boolean skip) {
        mSkipAsyncCheckForTesting = skip;
    }

    @VisibleForTesting
    static void reset() {
        sInstance = null;
    }

    /**
     * Returns, via callback, the ownded state for native's AndroidEnterpriseInfo.
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

        EnterpriseInfo.getInstance().getDeviceEnterpriseInfo(callback);
    }

    @NativeMethods
    interface Natives {
        void updateNativeOwnedState(boolean hasProfileOwnerApp, boolean hasDeviceOwnerApp);
    }
}
