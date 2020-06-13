// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.TimeUtilsJni;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.indicator.ConnectivityDetector.ConnectionState;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Class that controls visibility and content of {@link StatusIndicatorCoordinator} to relay
 * connectivity information.
 */
public class OfflineIndicatorControllerV2 implements ConnectivityDetector.Observer {
    @IntDef({UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS,
            UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED,
            UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS,
            UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UmaEnum {
        int CAN_ANIMATE_NATIVE_CONTROLS = 0;
        int CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED = 1;
        int CANNOT_ANIMATE_NATIVE_CONTROLS = 2;
        int CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED = 3;

        int NUM_ENTRIES = 4;
    }

    static final long STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS = 2000;
    static final long STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS = 5000;

    private static ConnectivityDetector sMockConnectivityDetector;
    private static Supplier<Long> sMockElapsedTimeSupplier;

    private Context mContext;
    private StatusIndicatorCoordinator mStatusIndicator;
    private Handler mHandler;
    private ConnectivityDetector mConnectivityDetector;
    private ObservableSupplier<Boolean> mIsUrlBarFocusedSupplier;
    private Supplier<Boolean> mCanAnimateBrowserControlsSupplier;
    private Callback<Boolean> mOnUrlBarFocusChanged;
    private Runnable mShowRunnable;
    private Runnable mUpdateAndHideRunnable;
    private Runnable mHideRunnable;
    private Runnable mOnUrlBarUnfocusedRunnable;
    private Runnable mUpdateStatusIndicatorDelayedRunnable;
    private long mLastActionTime;
    private boolean mIsOffline;
    private long mTimeShownMs;

    /**
     * Constructs the offline indicator.
     * @param context The {@link Context}.
     * @param statusIndicator The {@link StatusIndicatorCoordinator} instance this controller will
     *                        control based on the connectivity.
     * @param isUrlBarFocusedSupplier The {@link ObservableSupplier} that will supply the UrlBar's
     *                                focus state and notify a listener when it changes.
     * @param canAnimateNativeBrowserControls Will supply a boolean meaning whether the native
     *                                        browser controls can be animated. This is used for
     *                                        collecting metrics.
     * TODO(sinansahin): We can remove canAnimateNativeBrowserControls once we're done with metrics
     *                   collection.
     */
    public OfflineIndicatorControllerV2(Context context, StatusIndicatorCoordinator statusIndicator,
            ObservableSupplier<Boolean> isUrlBarFocusedSupplier,
            Supplier<Boolean> canAnimateNativeBrowserControls) {
        mContext = context;
        mStatusIndicator = statusIndicator;
        mHandler = new Handler();

        // If we're offline at start-up, we should have a small enough last action time so that we
        // don't wait for the cool-down.
        mLastActionTime = getElapsedTime() - STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS;

        mShowRunnable = () -> {
            RecordUserAction.record("OfflineIndicator.Shown");
            mTimeShownMs = TimeUnit.MICROSECONDS.toMillis(TimeUtilsJni.get().getTimeTicksNowUs());

            setLastActionTime();

            final int backgroundColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.offline_indicator_offline_color);
            final int textColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_text_color_light);
            final Drawable statusIcon = mContext.getDrawable(R.drawable.ic_cloud_offline_24dp);
            final int iconTint = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_icon_color_light);
            mStatusIndicator.show(mContext.getString(R.string.offline_indicator_v2_offline_text),
                    statusIcon, backgroundColor, textColor, iconTint);
        };

        mHideRunnable = () -> {
            mHandler.postDelayed(
                    () -> mStatusIndicator.hide(), STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS);
        };

        mUpdateAndHideRunnable = () -> {
            RecordUserAction.record("OfflineIndicator.Hidden");
            final long shownDuration =
                    TimeUnit.MICROSECONDS.toMillis(TimeUtilsJni.get().getTimeTicksNowUs())
                    - mTimeShownMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "OfflineIndicator.ShownDuration", shownDuration);

            setLastActionTime();

            final int backgroundColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.offline_indicator_back_online_color);
            final int textColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_text_color_inverse);
            final Drawable statusIcon = mContext.getDrawable(R.drawable.ic_globe_24dp);
            final int iconTint = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_icon_color_inverse);
            mStatusIndicator.updateContent(
                    mContext.getString(R.string.offline_indicator_v2_back_online_text), statusIcon,
                    backgroundColor, textColor, iconTint, mHideRunnable);
        };

        mIsUrlBarFocusedSupplier = isUrlBarFocusedSupplier;
        mCanAnimateBrowserControlsSupplier = canAnimateNativeBrowserControls;
        // TODO(crbug.com/1075793): Move the UrlBar focus related code to the widget or glue code.
        mOnUrlBarFocusChanged = (hasFocus) -> {
            if (!hasFocus && mOnUrlBarUnfocusedRunnable != null) {
                mOnUrlBarUnfocusedRunnable.run();
                mOnUrlBarUnfocusedRunnable = null;
            }
        };
        mIsUrlBarFocusedSupplier.addObserver(mOnUrlBarFocusChanged);

        mUpdateStatusIndicatorDelayedRunnable = () -> {
            final boolean offline =
                    isConnectionStateOffline(mConnectivityDetector.getConnectionState());
            if (offline != mIsOffline) {
                updateStatusIndicator(offline);
            }
        };

        if (sMockConnectivityDetector != null) {
            mConnectivityDetector = sMockConnectivityDetector;
        } else {
            mConnectivityDetector = new ConnectivityDetector(this);
        }
    }

    @Override
    public void onConnectionStateChanged(int connectionState) {
        final boolean offline = isConnectionStateOffline(connectionState);
        if (mIsOffline == offline) {
            return;
        }

        mHandler.removeCallbacks(mUpdateStatusIndicatorDelayedRunnable);
        // TODO(crbug.com/1081427): This currently only protects the widget from going into a bad
        // state. We need a better way to handle flaky connections.
        final long elapsedTimeSinceLastAction = getElapsedTime() - mLastActionTime;
        if (elapsedTimeSinceLastAction < STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS) {
            mHandler.postDelayed(mUpdateStatusIndicatorDelayedRunnable,
                    STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - elapsedTimeSinceLastAction);
            return;
        }

        updateStatusIndicator(offline);
    }

    public void destroy() {
        if (mConnectivityDetector != null) {
            mConnectivityDetector.destroy();
            mConnectivityDetector = null;
        }

        if (mIsUrlBarFocusedSupplier != null) {
            mIsUrlBarFocusedSupplier.removeObserver(mOnUrlBarFocusChanged);
            mIsUrlBarFocusedSupplier = null;
        }

        mOnUrlBarFocusChanged = null;

        mHandler.removeCallbacks(mHideRunnable);
        mHandler.removeCallbacks(mUpdateStatusIndicatorDelayedRunnable);
    }

    private void updateStatusIndicator(boolean offline) {
        mIsOffline = offline;
        int surfaceState;
        if (mIsUrlBarFocusedSupplier.get()) {
            // We should clear the runnable if we would be assigning an unnecessary show or hide
            // runnable. E.g, without this condition, we would be trying to hide the indicator when
            // it's not shown if we were set to show the widget but then went back online.
            if ((!offline && mOnUrlBarUnfocusedRunnable == mShowRunnable)
                    || (offline && mOnUrlBarUnfocusedRunnable == mUpdateAndHideRunnable)) {
                mOnUrlBarUnfocusedRunnable = null;
                return;
            }
            mOnUrlBarUnfocusedRunnable = offline ? mShowRunnable : mUpdateAndHideRunnable;
            surfaceState = mCanAnimateBrowserControlsSupplier.get()
                    ? UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED
                    : UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED;
        } else {
            assert mOnUrlBarUnfocusedRunnable == null;
            (offline ? mShowRunnable : mUpdateAndHideRunnable).run();
            surfaceState = mCanAnimateBrowserControlsSupplier.get()
                    ? UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS
                    : UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS;
            ;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "OfflineIndicator.ConnectivityChanged.DeviceState."
                        + (offline ? "Offline" : "Online"),
                surfaceState, UmaEnum.NUM_ENTRIES);
    }

    private long getElapsedTime() {
        return sMockElapsedTimeSupplier != null ? sMockElapsedTimeSupplier.get()
                                                : SystemClock.elapsedRealtime();
    }

    private void setLastActionTime() {
        mLastActionTime = getElapsedTime();
    }

    private boolean isConnectionStateOffline(@ConnectionState int connectionState) {
        return connectionState != ConnectionState.VALIDATED;
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
