// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.notification;

import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTextBody;
import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTitle;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_AVAILABLE;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;
import static org.chromium.chrome.browser.omaha.notification.UpdateNotificationControllerImpl.PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY;

import android.content.Intent;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.omaha.OmahaBase;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

/**
 * Class supports to build and to send update notification per certain duration if new Chrome
 * version is available. It listens to {@link UpdateStatusProvider}, and coordinates with the
 * backend notification scheduling system.
 */
@JNINamespace("updates")
public class UpdateNotificationServiceBridge implements UpdateNotificationController, Destroyable {
    private static final String PREF_UPDATE_NOTIFICATION_THROTTLE_INTERVAL_KEY =
            "pref_update_notification_throttle_interval_key";
    private static final String PREF_UPDATE_NOTIFICATION_USER_DISMISS_COUNT_KEY =
            "pref_update_notification_user_dismiss_count_key";

    private final Callback<UpdateStatusProvider.UpdateStatus> mObserver = status -> {
        mUpdateStatus = status;
        processUpdateStatus();
    };

    private ChromeActivity mActivity;
    private @Nullable UpdateStatusProvider.UpdateStatus mUpdateStatus;
    private static final String TAG = "cr_UpdateNotif";

    /**
     * @param activity A {@link ChromeActivity} instance the notification will be shown in.
     */
    public UpdateNotificationServiceBridge(ChromeActivity activity) {
        mActivity = activity;
        UpdateStatusProvider.getInstance().addObserver(mObserver);
        mActivity.getLifecycleDispatcher().register(this);
    }

    // UpdateNotificationController implementation.
    @Override
    public void onNewIntent(Intent intent) {
        processUpdateStatus();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        UpdateStatusProvider.getInstance().removeObserver(mObserver);
        mActivity.getLifecycleDispatcher().unregister(this);
        mActivity = null;
    }

    private void processUpdateStatus() {
        if (mUpdateStatus == null) return;
        switch (mUpdateStatus.updateState) {
            case UPDATE_AVAILABLE: // Intentional fallthrough.
            case INLINE_UPDATE_AVAILABLE: // Intentional fallthrough.
                boolean shouldShowImmediately = mUpdateStatus.updateState == INLINE_UPDATE_AVAILABLE
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.UPDATE_NOTIFICATION_IMMEDIATE_SHOW_OPTION);
                UpdateNotificationServiceBridgeJni.get().schedule(getUpdateNotificationTitle(),
                        getUpdateNotificationTextBody(), mUpdateStatus.updateState,
                        shouldShowImmediately);

                break;
            default:
                break;
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Schedule a notification through scheduling system.
         * @param title The title string of notification context.
         * @param message The body string of notification context.
         * @param state An enum of {@link UpdateState} pulled from UpdateStatusProvider.
         * @param shouldShowImmediately A flag to show notification right away if it is true.
         */
        void schedule(String title, String message, @UpdateState int state,
                boolean shouldShowImmediately);
    }

    @CalledByNative
    public static long getLastShownTimeStamp() {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        return preferences.getLong(PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY, 0);
    }

    @CalledByNative
    private static void updateLastShownTimeStamp(long timestamp) {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putLong(PREF_LAST_TIME_UPDATE_NOTIFICATION_KEY, timestamp);
        editor.apply();
    }

    /**
     * Gets the throttle interval in milliseconds from {@link SharedPreferences}.
     * return 0 if not exists.
     */
    @CalledByNative
    public static long getThrottleInterval() {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        return preferences.getLong(PREF_UPDATE_NOTIFICATION_THROTTLE_INTERVAL_KEY, 0);
    }

    /**
     * Updates the throttle interval to show Chrome update notification in {@link
     * SharedPreferences}.
     * @param interval Throttle interval in milliseconds.
     */
    @CalledByNative
    private static void updateThrottleInterval(long interval) {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putLong(PREF_UPDATE_NOTIFICATION_THROTTLE_INTERVAL_KEY, interval);
        editor.apply();
    }

    /**
     * Gets the number of users consecutive dismiss or negative button action on update notification
     * from {@link SharedPreferences}.
     */
    @CalledByNative
    public static int getNegativeActionCount() {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        return preferences.getInt(PREF_UPDATE_NOTIFICATION_USER_DISMISS_COUNT_KEY, 0);
    }

    /**
     * Updates the number to record users consecutive dismiss or negative button click action on
     * update notification in {@link SharedPreferences}.
     * @param count A number of users consecutive dimiss action.
     */
    @CalledByNative
    private static void updateNegativeActionCount(int count) {
        SharedPreferences preferences = OmahaBase.getSharedPreferences();
        SharedPreferences.Editor editor = preferences.edit();
        editor.putInt(PREF_UPDATE_NOTIFICATION_USER_DISMISS_COUNT_KEY, count);
        editor.apply();
    }

    /**
     * Launches Chrome activity depends on {@link UpdateState}.
     * @param state An enum value of {@link UpdateState} stored in native side schedule service.
     * */
    @CalledByNative
    private static void launchChromeActivity(@UpdateState int state) {
        try {
            RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.Notification.LaunchEvent",
                    UpdateNotificationControllerImpl.UpdateNotificationReceiver.LaunchEvent.START,
                    UpdateNotificationControllerImpl.UpdateNotificationReceiver.LaunchEvent
                            .NUM_ENTRIES);
            UpdateUtils.onUpdateAvailable(ContextUtils.getApplicationContext(), state);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Failed to start activity in background.", e);
            RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.Notification.LaunchEvent",
                    UpdateNotificationControllerImpl.UpdateNotificationReceiver.LaunchEvent
                            .START_ACTIVITY_FAILED,
                    UpdateNotificationControllerImpl.UpdateNotificationReceiver.LaunchEvent
                            .NUM_ENTRIES);
        }
    }
}
