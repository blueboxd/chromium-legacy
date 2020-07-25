// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.policy.AppRestrictionsProvider;

import java.util.LinkedList;
import java.util.Locale;
import java.util.Queue;
import java.util.concurrent.RejectedExecutionException;

/**
 * A helper class used to check if app restrictions are present during first run. Internally it
 * uses an asynchronous background task to fetch restrictions, then notifies registered callbacks
 * once complete.
 *
 * This class is only used during first run flow, so its lifecycle ends when first run completes.
 */
class FirstRunAppRestrictionInfo {
    private static final String TAG = "FRAppRestrictionInfo";

    private static FirstRunAppRestrictionInfo sInstance;

    private boolean mInitialized;
    private boolean mIsFetching;
    private boolean mHasAppRestriction;
    private Queue<Callback<Boolean>> mCallBacks = new LinkedList<>();

    private AsyncTask<Boolean> mFetchAppRestrictionAsyncTask;
    private final AppRestrictionsProvider mProvider;

    private FirstRunAppRestrictionInfo() {
        // TODO(crbug.com/1106407): A new AppRestrictionProvider is used here to simplify the
        //  initial implementation. If the future we should look at sharing a single
        //  AppRestrictionProvider in the main policy code to avoid redundant calls to Android APIs
        //  to load policies.
        mProvider = new AppRestrictionsProvider(ContextUtils.getApplicationContext()) {
            @Override
            public void notifySettingsAvailable(Bundle settings) {
                // Update the singleton when we heard update from policy
                // No super method here as we do not want to invoke calls to CombinedPolicyProvider.
                ThreadUtils.assertOnUiThread();
                onRestrictionDetected(!settings.isEmpty(), 0);
            }
        };
    }

    @VisibleForTesting
    FirstRunAppRestrictionInfo(@Nullable AppRestrictionsProvider provider) {
        mProvider = provider;
    }

    public static FirstRunAppRestrictionInfo getInstance() {
        // Only access the singleton on UI thread for thread safety.
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new FirstRunAppRestrictionInfo();
        return sInstance;
    }

    /**
     * Destroy the singleton instance, stop async initialization if it is in progress, and remove
     * all the callbacks.
     */
    public static void destroy() {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) {
            if (sInstance.mFetchAppRestrictionAsyncTask != null) {
                sInstance.mFetchAppRestrictionAsyncTask.cancel(true);
            }
            if (sInstance.mProvider != null) {
                sInstance.mProvider.destroy();
            }
            sInstance.mCallBacks.clear();
        }
        sInstance = null;
    }

    /**
     * Register a callback whether app restriction is found on device. If app restrictions have
     * already been fetched, the callback will be invoked immediately.
     *
     * @param callback Callback to run with whether app restriction is found on device.
     */
    public void getHasAppRestriction(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        if (mInitialized) {
            callback.onResult(mHasAppRestriction);
        } else {
            mCallBacks.add(callback);
        }
    }

    /**
     * Start fetching app restriction on an async thread.
     */
    public void initialize() {
        ThreadUtils.assertOnUiThread();
        // Early out if info is already fetched or any job has started.
        if (mInitialized || mIsFetching) return;

        mIsFetching = true;

        Context appContext = ContextUtils.getApplicationContext();
        long startTime = SystemClock.elapsedRealtime();
        try {
            mFetchAppRestrictionAsyncTask = new AsyncTask<Boolean>() {
                @Override
                protected Boolean doInBackground() {
                    UserManager userManager =
                            (UserManager) appContext.getSystemService(Context.USER_SERVICE);
                    Bundle bundle =
                            AppRestrictionsProvider.getApplicationRestrictionsFromUserManager(
                                    userManager, appContext.getPackageName());
                    return !bundle.isEmpty();
                }

                @Override
                protected void onPostExecute(Boolean isAppRestricted) {
                    onRestrictionDetected(isAppRestricted, startTime);
                }
            };
            mFetchAppRestrictionAsyncTask.executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
        } catch (RejectedExecutionException e) {
            // Though unlikely, if the task is rejected, we assume no restriction exists.
            onRestrictionDetected(false, startTime);
        }

        if (mProvider != null) mProvider.startListeningForPolicyChanges();
    }

    private void onRestrictionDetected(boolean isAppRestricted, long startTime) {
        mHasAppRestriction = isAppRestricted;
        mInitialized = true;

        // Only record histogram when startTime is valid.
        if (startTime > 0) {
            long runTime = SystemClock.elapsedRealtime() - startTime;
            RecordHistogram.recordTimesHistogram(
                    "Enterprise.FirstRun.AppRestrictionLoadTime", runTime);
            Log.d(TAG,
                    String.format(Locale.US, "Policy received. Runtime: [%d], result: [%s]",
                            runTime, isAppRestricted));
        }

        while (!mCallBacks.isEmpty()) {
            mCallBacks.remove().onResult(mHasAppRestriction);
        }
    }
}
