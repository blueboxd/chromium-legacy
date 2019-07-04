// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.MenuOrKeyboardActionController;
import org.chromium.chrome.browser.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.appmenu.AppMenuCoordinatorFactory;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarObserver;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * The root UI coordinator. This class will eventually be responsible for inflating and managing
 * lifecycle of the main UI components.
 *
 * The specific things this component will manage and how it will hook into Chrome*Activity are
 * still being discussed See https://crbug.com/931496.
 */
public class RootUiCoordinator
        implements Destroyable, InflationObserver,
                   MenuOrKeyboardActionController.MenuOrKeyboardActionHandler, AppMenuBlocker {
    protected ChromeActivity mActivity;
    protected @Nullable AppMenuCoordinator mAppMenuCoordinator;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    protected @Nullable FindToolbarManager mFindToolbarManager;
    private @Nullable FindToolbarObserver mFindToolbarObserver;

    private Callback<LayoutManager> mLayoutManagerSupplierCallback;
    private OverlayPanelManager mOverlayPanelManager;
    private OverlayPanelManager.OverlayPanelManagerObserver mOverlayPanelManagerObserver;

    private Callback<OverviewModeBehavior> mOverviewModeBehaviorSupplierCallback;
    private OverviewModeBehavior mOverviewModeBehavior;
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    private VrModeObserver mVrModeObserver;

    /**
     * Create a new {@link RootUiCoordinator} for the given activity.
     * @param activity The containing {@link ChromeActivity}. TODO(https://crbug.com/931496):
     *         Remove this in favor of passing in direct dependencies.
     */
    public RootUiCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mActivity.getLifecycleDispatcher().register(this);

        mMenuOrKeyboardActionController = mActivity.getMenuOrKeyboardActionController();
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(this);

        initLayoutManagerSupplierObserver();
        initOverviewModeSupplierObserver();
    }

    @Override
    public void destroy() {
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(this);

        mActivity.getLayoutManagerSupplier().removeObserver(mLayoutManagerSupplierCallback);
        if (mOverlayPanelManager != null) {
            mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
        }

        if (mActivity.getOverviewModeBehaviorSupplier() != null) {
            mActivity.getOverviewModeBehaviorSupplier().removeObserver(
                    mOverviewModeBehaviorSupplierCallback);
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }

        if (mAppMenuCoordinator != null) {
            mAppMenuCoordinator.unregisterAppMenuBlocker(this);
            mAppMenuCoordinator.unregisterAppMenuBlocker(mActivity);
            mAppMenuCoordinator.destroy();
        }

        if (mFindToolbarManager != null) mFindToolbarManager.removeObserver(mFindToolbarObserver);

        if (mVrModeObserver != null) VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);

        mActivity = null;
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        initAppMenu();
        initFindToolbarManager();

        mVrModeObserver = new VrModeObserver() {
            @Override
            public void onEnterVr() {
                mFindToolbarManager.hideToolbar();
            }

            @Override
            public void onExitVr() {}
        };
        VrModuleProvider.registerVrModeObserver(mVrModeObserver);
    }

    // MenuOrKeyboardActionHandler implementation

    @Override
    public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu && mAppMenuCoordinator != null) {
            mAppMenuCoordinator.showAppMenuForKeyboardEvent();
            return true;
        } else if (id == R.id.find_in_page_id) {
            if (mFindToolbarManager == null) return false;

            mFindToolbarManager.showToolbar();

            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        }

        return false;
    }

    // AppMenuBlocker implementation

    @Override
    public boolean canShowAppMenu() {
        // TODO(https:crbug.com/931496): Eventually the ContextualSearchManager, EphemeralTabPanel,
        // and FindToolbarManager will all be owned by this class.

        // Do not show the menu if Contextual Search panel is opened.
        if (mActivity.getContextualSearchManager() != null
                && mActivity.getContextualSearchManager().isSearchPanelOpened()) {
            return false;
        }

        if (mActivity.getEphemeralTabPanel() != null
                && mActivity.getEphemeralTabPanel().isPanelOpened()) {
            return false;
        }

        // Do not show the menu if we are in find in page view.
        if (mFindToolbarManager != null && mFindToolbarManager.isShowing()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return false;
        }

        return true;
    }

    // Private class methods

    private void initLayoutManagerSupplierObserver() {
        mLayoutManagerSupplierCallback = layoutManager -> {
            if (mOverlayPanelManager != null) {
                mOverlayPanelManager.removeObserver(mOverlayPanelManagerObserver);
            }
            mOverlayPanelManager = layoutManager.getOverlayPanelManager();

            if (mOverlayPanelManagerObserver == null) {
                mOverlayPanelManagerObserver =
                        new OverlayPanelManager.OverlayPanelManagerObserver() {
                            @Override
                            public void onOverlayPanelShown() {
                                if (mFindToolbarManager != null) {
                                    mFindToolbarManager.hideToolbar(false);
                                }
                            }

                            @Override
                            public void onOverlayPanelHidden() {}
                        };
            }

            mOverlayPanelManager.addObserver(mOverlayPanelManagerObserver);
        };
        mActivity.getLayoutManagerSupplier().addObserver(mLayoutManagerSupplierCallback);
    }

    private void initOverviewModeSupplierObserver() {
        if (mActivity.getOverviewModeBehaviorSupplier() != null) {
            mOverviewModeBehaviorSupplierCallback = overviewModeBehavior -> {
                if (mOverviewModeBehavior != null) {
                    mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
                }

                mOverviewModeBehavior = overviewModeBehavior;

                if (mOverviewModeObserver == null) {
                    mOverviewModeObserver = new OverviewModeBehavior.OverviewModeObserver() {
                        @Override
                        public void onOverviewModeStartedShowing(boolean showToolbar) {
                            if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
                        }

                        @Override
                        public void onOverviewModeFinishedShowing() {}

                        @Override
                        public void onOverviewModeStartedHiding(
                                boolean showToolbar, boolean delayAnimation) {}

                        @Override
                        public void onOverviewModeFinishedHiding() {}
                    };
                }
                mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            };
            mActivity.getOverviewModeBehaviorSupplier().addObserver(
                    mOverviewModeBehaviorSupplierCallback);
        }
    }

    private void initAppMenu() {
        // TODO(https://crbug.com/931496): Revisit this as part of the broader
        // discussion around activity-specific UI customizations.
        if (mActivity.supportsAppMenu()) {
            mAppMenuCoordinator = AppMenuCoordinatorFactory.createAppMenuCoordinator(mActivity,
                    mActivity.getLifecycleDispatcher(), mActivity.getToolbarManager(), mActivity,
                    mActivity.getWindow().getDecorView(),
                    mActivity.getOverviewModeBehaviorSupplier());
            mActivity.getToolbarManager().onAppMenuInitialized(
                    mAppMenuCoordinator.getAppMenuHandler(),
                    mAppMenuCoordinator.getAppMenuPropertiesDelegate());
            mAppMenuCoordinator.registerAppMenuBlocker(this);
            mAppMenuCoordinator.registerAppMenuBlocker(mActivity);
        } else if (mActivity.getToolbarManager() != null) {
            mActivity.getToolbarManager().getToolbar().disableMenuButton();
        }
    }

    private void initFindToolbarManager() {
        if (!mActivity.supportsFindInPage()) return;

        int stubId = R.id.find_toolbar_stub;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            stubId = R.id.find_toolbar_tablet_stub;
        }
        mFindToolbarManager = new FindToolbarManager(mActivity.findViewById(stubId),
                mActivity.getTabModelSelector(), mActivity.getWindowAndroid(),
                mActivity.getToolbarManager().getActionModeControllerCallback());

        mFindToolbarObserver = new FindToolbarObserver() {
            @Override
            public void onFindToolbarShown() {
                if (mActivity.getContextualSearchManager() != null) {
                    mActivity.getContextualSearchManager().hideContextualSearch(
                            OverlayPanel.StateChangeReason.UNKNOWN);
                }
                if (mActivity.getEphemeralTabPanel() != null) {
                    mActivity.getEphemeralTabPanel().closePanel(
                            OverlayPanel.StateChangeReason.UNKNOWN, true);
                }
            }

            @Override
            public void onFindToolbarHidden() {}
        };

        mFindToolbarManager.addObserver(mFindToolbarObserver);

        mActivity.getToolbarManager().setFindToolbarManager(mFindToolbarManager);
    }

    // Testing methods

    @VisibleForTesting
    public AppMenuCoordinator getAppMenuCoordinatorForTesting() {
        return mAppMenuCoordinator;
    }
}
