// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.util.TokenHolder;

/**
 * Handles app menu logic for the toolbar, e.g. showing/hiding the app update badge.
 */
class ToolbarAppMenuManager implements AppMenuObserver {
    interface SetFocusFunction {
        void setFocus(boolean focus, int reason);
    }

    private ObservableSupplier<AppMenuCoordinator> mAppMenuCoordinatorSupplier;
    private Callback<AppMenuCoordinator> mAppMenuCoordinatorSupplierObserver;
    private @Nullable AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuButtonHelper mAppMenuButtonHelper;
    private AppMenuHandler mAppMenuHandler;
    private final BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private final Activity mActivity;
    private int mFullscreenMenuToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenHighlightToken = TokenHolder.INVALID_TOKEN;
    private final TopToolbarCoordinator mToolbar;
    private final SetFocusFunction mSetUrlBarFocusFunction;
    private Runnable mRequestRenderRunnable;
    private Runnable mUpdateStateChangedListener;
    private final boolean mShouldShowAppUpdateBadge;
    private Supplier<Boolean> mIsInOverviewModeSupplier;

    /**
     *
     * @param appMenuCoordinatorSupplier Supplier for the AppMenuCoordinator, which owns all other
     *         app menu MVC components.
     * @param controlsVisibilityDelegate Delegate for forcing persistent display of browser
     *         controls.
     * @param activity Activity in which this object lives.
     * @param toolbar Toolbar object that hosts the app menu button.
     * @param setUrlBarFocusFunction Function that allows setting focus on the url bar.
     * @param requestRenderRunnable Runnable that requests a re-rendering of the compositor view
     *         containing the app menu button.
     * @param shouldShowAppUpdateBadge Whether the app menu update badge should be shown if there is
     *         a pending update.
     */
    public ToolbarAppMenuManager(ObservableSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            Activity activity, TopToolbarCoordinator toolbar,
            SetFocusFunction setUrlBarFocusFunction, Runnable requestRenderRunnable,
            boolean shouldShowAppUpdateBadge, Supplier<Boolean> isInOverviewModeSupplier) {
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        mActivity = activity;
        mToolbar = toolbar;
        mSetUrlBarFocusFunction = setUrlBarFocusFunction;
        mAppMenuCoordinatorSupplier = appMenuCoordinatorSupplier;
        mAppMenuCoordinatorSupplierObserver = this::onAppMenuInitialized;
        appMenuCoordinatorSupplier.addObserver(mAppMenuCoordinatorSupplierObserver);
        mRequestRenderRunnable = requestRenderRunnable;
        mShouldShowAppUpdateBadge = shouldShowAppUpdateBadge;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
    }

    public void updateReloadingState(boolean isLoading) {
        if (mAppMenuPropertiesDelegate == null || mAppMenuHandler == null) return;
        mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
        mAppMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
    }

    public void destroy() {
        if (mAppMenuButtonHelper != null) {
            mAppMenuHandler.removeObserver(this);
            mAppMenuButtonHelper = null;
        }

        if (mUpdateStateChangedListener != null) {
            UpdateMenuItemHelper.getInstance().unregisterObserver(mUpdateStateChangedListener);
            mUpdateStateChangedListener = null;
        }
    }

    @Override
    public void onMenuVisibilityChanged(boolean isVisible) {
        if (isVisible) {
            // Defocus here to avoid handling focus in multiple places, e.g., when the
            // forward button is pressed. (see crbug.com/414219)
            mSetUrlBarFocusFunction.setFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

            if (!mIsInOverviewModeSupplier.get() && isShowingAppMenuUpdateBadge()) {
                // The app menu badge should be removed the first time the menu is opened.
                mToolbar.removeAppMenuUpdateBadge(true);
                mRequestRenderRunnable.run();
            }

            mFullscreenMenuToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenMenuToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenMenuToken);
        }

        MenuButton menuButton = getMenuButtonWrapper();
        if (isVisible && menuButton != null && menuButton.isShowingAppMenuUpdateBadge()) {
            UpdateMenuItemHelper.getInstance().onMenuButtonClicked();
        }
    }

    @Override
    public void onMenuHighlightChanged(boolean isHighlighting) {
        final MenuButton menuButton = getMenuButtonWrapper();
        if (menuButton != null) menuButton.setMenuButtonHighlight(isHighlighting);

        if (isHighlighting) {
            mFullscreenHighlightToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenHighlightToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenHighlightToken);
        }
    }

    void onNativeInitialized() {
        if (mShouldShowAppUpdateBadge) {
            mUpdateStateChangedListener = this::updateStateChanged;
            UpdateMenuItemHelper.getInstance().registerObserver(mUpdateStateChangedListener);
        }
    }

    @Nullable
    AppMenuButtonHelper getMenuButtonHelper() {
        return mAppMenuButtonHelper;
    }

    /**
     * Called when the app menu and related properties delegate are available.
     *
     * @param appMenuCoordinator The coordinator for interacting with the menu.
     */
    private void onAppMenuInitialized(AppMenuCoordinator appMenuCoordinator) {
        assert mAppMenuHandler == null;
        AppMenuHandler appMenuHandler = appMenuCoordinator.getAppMenuHandler();

        mAppMenuHandler = appMenuHandler;
        mAppMenuHandler.addObserver(this);
        mAppMenuButtonHelper = mAppMenuHandler.createAppMenuButtonHelper();
        mAppMenuButtonHelper.setOnAppMenuShownListener(() -> {
            RecordUserAction.record("MobileToolbarShowMenu");
            mToolbar.onMenuShown();
        });
        mToolbar.setAppMenuButtonHelper(mAppMenuButtonHelper);
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();

        // TODO(pnoland, https://crbug.com/1084528): replace this with a one shot supplier so we can
        // express that we don't handle the menu coordinator being set more than once.
        mAppMenuCoordinatorSupplier.removeObserver(mAppMenuCoordinatorSupplierObserver);
        mAppMenuCoordinatorSupplier = null;
        mAppMenuCoordinatorSupplierObserver = null;
    }

    @Nullable
    private MenuButton getMenuButtonWrapper() {
        return mToolbar.getMenuButtonWrapper();
    }

    /**
     * @return Whether the badge is showing (either in the toolbar).
     */
    private boolean isShowingAppMenuUpdateBadge() {
        return mToolbar.isShowingAppMenuUpdateBadge();
    }

    @VisibleForTesting
    void updateStateChanged() {
        if (mActivity.isFinishing() || mActivity.isDestroyed() || !mShouldShowAppUpdateBadge) {
            return;
        }

        UpdateMenuItemHelper.MenuButtonState buttonState =
                UpdateMenuItemHelper.getInstance().getUiState().buttonState;
        if (buttonState != null) {
            mToolbar.showAppMenuUpdateBadge();
            mRequestRenderRunnable.run();
        } else {
            mToolbar.removeAppMenuUpdateBadge(false);
        }
    }
}
