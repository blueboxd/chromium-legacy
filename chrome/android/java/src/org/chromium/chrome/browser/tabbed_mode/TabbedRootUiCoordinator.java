// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import androidx.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.tablet.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI.
 */
public class TabbedRootUiCoordinator extends RootUiCoordinator implements NativeInitObserver {
    private @Nullable ImmersiveModeManager mImmersiveModeManager;
    private TabbedSystemUiCoordinator mSystemUiCoordinator;
    private @Nullable EmptyBackgroundViewWrapper mEmptyBackgroundViewWrapper;

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorCoordinator.StatusIndicatorObserver mStatusIndicatorObserver;

    public TabbedRootUiCoordinator(ChromeActivity activity) {
        super(activity);
    }

    @Override
    public void destroy() {
        if (mImmersiveModeManager != null) mImmersiveModeManager.destroy();
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mEmptyBackgroundViewWrapper != null) mEmptyBackgroundViewWrapper.destroy();

        if (mStatusIndicatorCoordinator != null) {
            mStatusIndicatorCoordinator.removeObserver(mStatusIndicatorObserver);
        }

        super.destroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        mImmersiveModeManager = AppHooks.get().createImmersiveModeManager(
                mActivity.getWindow().getDecorView().findViewById(android.R.id.content));
        mSystemUiCoordinator = new TabbedSystemUiCoordinator(mActivity.getWindow(),
                mActivity.getTabModelSelector(), mImmersiveModeManager,
                mActivity.getOverviewModeBehaviorSupplier());

        if (mImmersiveModeManager != null) {
            mActivity.getToolbarManager().setImmersiveModeManager(mImmersiveModeManager);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        // TODO(twellington): Supply TabModelSelector as well and move initialization earlier.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            AppMenuHandler appMenuHandler =
                    mAppMenuCoordinator == null ? null : mAppMenuCoordinator.getAppMenuHandler();
            mEmptyBackgroundViewWrapper = new EmptyBackgroundViewWrapper(
                    mActivity.getTabModelSelector(), mActivity.getTabCreator(false), mActivity,
                    appMenuHandler, mActivity.getSnackbarManager(),
                    mActivity.getOverviewModeBehaviorSupplier());
            mEmptyBackgroundViewWrapper.initialize();
        }
    }

    // Protected class methods

    @Override
    protected void onLayoutManagerAvailable(LayoutManager layoutManager) {
        super.onLayoutManagerAvailable(layoutManager);

        initStatusIndicatorCoordinator(layoutManager);
    }

    // Private class methods

    private void initStatusIndicatorCoordinator(LayoutManager layoutManager) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)) {
            return;
        }

        mStatusIndicatorCoordinator = new StatusIndicatorCoordinator(
                mActivity, mActivity.getCompositorViewHolder().getResourceManager());
        layoutManager.setStatusIndicatorSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver = (indicatorHeight -> {
            mActivity.getToolbarManager().setControlContainerTopMargin(indicatorHeight);
            layoutManager.getToolbarSceneLayer().setStaticYOffset(indicatorHeight);
            final int resourceId = mActivity.getControlContainerHeightResource();
            final int topControlsNewHeight =
                    mActivity.getResources().getDimensionPixelSize(resourceId) + indicatorHeight;
            mActivity.getFullscreenManager().setTopControlsHeight(topControlsNewHeight);
        });
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
    }

    @VisibleForTesting
    public StatusIndicatorCoordinator getStatusIndicatorCoordinatorForTesting() {
        return mStatusIndicatorCoordinator;
    }
}
