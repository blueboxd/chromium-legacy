// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.offlinepages.indicator.ConnectivityDetector.ConnectionState;

/**
 * Class that detects if the network is offline. Waits for the network to stablize before notifying
 * the observer.
 */
class OfflineDetector
        implements ConnectivityDetector.Observer, ApplicationStatus.ApplicationStateListener {
    static final long STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS = 2000;

    private static ConnectivityDetector sMockConnectivityDetector;
    private static Supplier<Long> sMockElapsedTimeSupplier;

    private ConnectivityDetector mConnectivityDetector;

    // Maintains if the connection is effectively offline.
    // Effectively offline means that all checks have been passed and the
    // |mCallback| has been invoked to notify the observers.
    private boolean mIsEffectivelyOffline;

    // True if the network is offline as detected by the connectivity detector.
    private boolean mIsOfflineLastReportedByConnectivityDetector;

    private Handler mHandler;
    private Runnable mUpdateOfflineStatusIndicatorDelayedRunnable;
    private final Callback<Boolean> mCallback;

    // Current state of the application.
    private int mApplicationState = ApplicationStatus.getStateForApplication();

    // Time when the application was last foregrounded. |callback| is invoked only when the app is
    // in foreground.
    private long mTimeWhenLastForegrounded;

    // Time when the connection was last reported as offline. |callback| is invoked only when the
    // connection has been in the ofline for |STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS|.
    private long mTimeWhenLastOffline;

    /**
     * Constructs the offline indicator.
     * @param callback The {@link callback} is invoked when the connectivity status is stable and
     *         has changed.
     */
    OfflineDetector(Callback<Boolean> callback) {
        mCallback = callback;
        mHandler = new Handler();

        mUpdateOfflineStatusIndicatorDelayedRunnable = () -> {
            // |callback| is invoked only when the app is in foreground. If the app is in
            // background, return early. When the app comes to foreground,
            // |mUpdateOfflineStatusIndicatorDelayedRunnable| would be posted.
            if (mApplicationState != ApplicationState.HAS_RUNNING_ACTIVITIES) {
                return;
            }

            // Connection state has not changed since |mUpdateOfflineStatusIndicatorDelayedRunnable|
            // was posted.
            if (mIsOfflineLastReportedByConnectivityDetector == mIsEffectivelyOffline) {
                return;
            }
            mIsEffectivelyOffline = mIsOfflineLastReportedByConnectivityDetector;
            mCallback.onResult(mIsEffectivelyOffline);
        };

        // Register as an application state observer and initialize |mTimeWhenLastForegrounded|.
        ApplicationStatus.registerApplicationStateListener(this);
        if (mApplicationState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            mTimeWhenLastForegrounded = getElapsedTime();
        }

        if (sMockConnectivityDetector != null) {
            mConnectivityDetector = sMockConnectivityDetector;
        } else {
            mConnectivityDetector = new ConnectivityDetector(this);
        }
    }

    @Override
    public void onConnectionStateChanged(int connectionState) {
        boolean previousLastReportedStateByOfflineDetector =
                mIsOfflineLastReportedByConnectivityDetector;
        mIsOfflineLastReportedByConnectivityDetector =
                (connectionState != ConnectionState.VALIDATED);
        if (previousLastReportedStateByOfflineDetector
                == mIsOfflineLastReportedByConnectivityDetector) {
            return;
        }

        if (mIsOfflineLastReportedByConnectivityDetector) {
            mTimeWhenLastOffline = getElapsedTime();
        }
        updateState();
    }

    /*
     * Returns true if the connection is offline and the connection state has been stable.
     */
    boolean isConnectionStateOffline() {
        return mIsEffectivelyOffline;
    }

    void destroy() {
        ApplicationStatus.unregisterApplicationStateListener(this);
        if (mConnectivityDetector != null) {
            mConnectivityDetector.destroy();
            mConnectivityDetector = null;
        }
        mHandler.removeCallbacks(mUpdateOfflineStatusIndicatorDelayedRunnable);
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (mApplicationState == newState) return;

        mApplicationState = newState;

        if (mApplicationState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            mTimeWhenLastForegrounded = getElapsedTime();
        }

        updateState();
    }

    private long getElapsedTime() {
        return sMockElapsedTimeSupplier != null ? sMockElapsedTimeSupplier.get()
                                                : SystemClock.elapsedRealtime();
    }

    @VisibleForTesting
    static void setMockConnectivityDetector(ConnectivityDetector connectivityDetector) {
        sMockConnectivityDetector = connectivityDetector;
    }

    @VisibleForTesting
    static void setMockElapsedTimeSupplier(Supplier<Long> supplier) {
        sMockElapsedTimeSupplier = supplier;
    }

    @VisibleForTesting
    void setHandlerForTesting(Handler handler) {
        mHandler = handler;
    }

    /*
    ** Calls |mUpdateOfflineStatusIndicatorDelayedRunnable| to update the connection state.
    */
    private void updateState() {
        mHandler.removeCallbacks(mUpdateOfflineStatusIndicatorDelayedRunnable);

        // Do not update state while the app is in background.
        if (mApplicationState != ApplicationState.HAS_RUNNING_ACTIVITIES) return;

        // Check time since the app was foregrounded and time since the offline notification was
        // received.
        final long timeSinceLastForeground = getElapsedTime() - mTimeWhenLastForegrounded;
        final long timeSinceOffline = getElapsedTime() - mTimeWhenLastOffline;
        final long timeNeededForForeground =
                STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS - timeSinceLastForeground;
        final long timeNeededForOffline =
                STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS - timeSinceOffline;

        assert mUpdateOfflineStatusIndicatorDelayedRunnable != null;

        // If the connection is online, report the state immediately. Alternatively, if the app has
        // been in foreground and connection has been offline for sufficient time, then report the
        // state immediately.
        if (!mIsOfflineLastReportedByConnectivityDetector
                || (timeNeededForForeground <= 0 && timeNeededForOffline <= 0)) {
            mUpdateOfflineStatusIndicatorDelayedRunnable.run();
            return;
        }

        // Wait before calling |mUpdateOfflineStatusIndicatorDelayedRunnable|.
        mHandler.postDelayed(mUpdateOfflineStatusIndicatorDelayedRunnable,
                Math.max(timeNeededForForeground, timeNeededForOffline));
    }
}
