// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.offlinepages.indicator.ConnectivityDetector.ConnectionState;

/**
 * Class that detects if the network is offline. Waits for the network to stablize before notifying
 * the observer.
 */
class OfflineDetector implements ConnectivityDetector.Observer {
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

    /**
     * Constructs the offline indicator.
     * @param callback The {@link callback} is invoked when the connectivity status is stable and
     *         has changed.
     */
    OfflineDetector(Callback<Boolean> callback) {
        mCallback = callback;
        mHandler = new Handler();

        if (sMockConnectivityDetector != null) {
            mConnectivityDetector = sMockConnectivityDetector;
        } else {
            mConnectivityDetector = new ConnectivityDetector(this);
        }

        mUpdateOfflineStatusIndicatorDelayedRunnable = () -> {
            // Connection state has not changed since |mUpdateOfflineStatusIndicatorDelayedRunnable|
            // was posted.
            if (mIsOfflineLastReportedByConnectivityDetector != mIsEffectivelyOffline) {
                mIsEffectivelyOffline = mIsOfflineLastReportedByConnectivityDetector;
                mCallback.onResult(mIsEffectivelyOffline);
            }
        };
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

        // Report the online state immediately if the connection is now online.
        if (!mIsOfflineLastReportedByConnectivityDetector) {
            mHandler.removeCallbacks(mUpdateOfflineStatusIndicatorDelayedRunnable);
            // Run the callback and report the connection as online if the last time we reported
            // connection as offline.
            if (mIsEffectivelyOffline) {
                mIsEffectivelyOffline = false;
                mCallback.onResult(mIsEffectivelyOffline);
            }
            return;
        }

        // Post a task to invoke the mCallback after some time.
        mHandler.postDelayed(mUpdateOfflineStatusIndicatorDelayedRunnable,
                STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
    }

    /*
     * Returns true if the connection is offline and the connection state has been stable.
     */
    boolean isConnectionStateOffline() {
        return mIsEffectivelyOffline;
    }

    void destroy() {
        if (mConnectivityDetector != null) {
            mConnectivityDetector.destroy();
            mConnectivityDetector = null;
        }
        mHandler.removeCallbacks(mUpdateOfflineStatusIndicatorDelayedRunnable);
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
}
