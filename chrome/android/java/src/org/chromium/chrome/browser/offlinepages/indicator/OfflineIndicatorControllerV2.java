// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;

/**
 * Class that controls visibility and content of {@link StatusIndicatorCoordinator} to relay
 * connectivity information.
 */
public class OfflineIndicatorControllerV2 implements ConnectivityDetector.Observer {
    private static final int STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS = 2000;

    private Context mContext;
    private StatusIndicatorCoordinator mStatusIndicator;
    private ConnectivityDetector mConnectivityDetector;
    private ObservableSupplier<Boolean> mIsUrlBarFocusedSupplier;
    private Callback<Boolean> mOnUrlBarFocusChanged;
    private Runnable mShowRunnable;
    private Runnable mHideRunnable;
    private Runnable mOnUrlBarUnfocusedRunnable;

    /**
     * Constructs the offline indicator.
     * @param context The {@link Context}.
     * @param statusIndicator The {@link StatusIndicatorCoordinator} instance this controller will
     *                        control based on the connectivity.
     * @param isUrlBarFocusedSupplier The {@link ObservableSupplier} that will supply the UrlBar's
     *                                focus state and notify a listener when it changes.
     */
    public OfflineIndicatorControllerV2(Context context, StatusIndicatorCoordinator statusIndicator,
            ObservableSupplier<Boolean> isUrlBarFocusedSupplier) {
        mContext = context;
        mStatusIndicator = statusIndicator;

        mShowRunnable = () -> {
            final int backgroundColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.offline_indicator_offline_color);
            final int textColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_text_color_light);
            final Drawable statusIcon = VectorDrawableCompat.create(
                    mContext.getResources(), R.drawable.ic_cloud_offline_24dp, mContext.getTheme());
            final int iconTint = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_icon_color_light);
            mStatusIndicator.show(mContext.getString(R.string.offline_indicator_v2_offline_text),
                    statusIcon, backgroundColor, textColor, iconTint);
        };

        mHideRunnable = () -> {
            final int backgroundColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.offline_indicator_back_online_color);
            final int textColor = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_text_color_inverse);
            final Drawable statusIcon = VectorDrawableCompat.create(
                    mContext.getResources(), R.drawable.ic_globe_24dp, mContext.getTheme());
            final int iconTint = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_icon_color_inverse);
            Runnable hide = () -> {
                final Handler handler = new Handler();
                handler.postDelayed(() -> mStatusIndicator.hide(),
                        STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS);
            };
            mStatusIndicator.updateContent(
                    mContext.getString(R.string.offline_indicator_v2_back_online_text), statusIcon,
                    backgroundColor, textColor, iconTint, hide);
        };

        mIsUrlBarFocusedSupplier = isUrlBarFocusedSupplier;
        // TODO(crbug.com/1075793): Move the UrlBar focus related code to the widget or glue code.
        mOnUrlBarFocusChanged = (hasFocus) -> {
            if (!hasFocus && mOnUrlBarUnfocusedRunnable != null) {
                mOnUrlBarUnfocusedRunnable.run();
                mOnUrlBarUnfocusedRunnable = null;
            }
        };
        mIsUrlBarFocusedSupplier.addObserver(mOnUrlBarFocusChanged);

        mConnectivityDetector = new ConnectivityDetector(this);
    }

    @Override
    public void onConnectionStateChanged(int connectionState) {
        final boolean offline = connectionState != ConnectivityDetector.ConnectionState.VALIDATED;

        if (mIsUrlBarFocusedSupplier.get()) {
            mOnUrlBarUnfocusedRunnable = offline ? mShowRunnable : mHideRunnable;
        } else {
            (offline ? mShowRunnable : mHideRunnable).run();
            assert mOnUrlBarUnfocusedRunnable == null;
        }
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
    }
}
