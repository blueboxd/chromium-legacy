// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;

/**
 * The root coordinator for the bottom toolbar. It has two sub-components: the browsing mode bottom
 * toolbar and the tab switcher mode bottom toolbar.
 */
class BottomToolbarCoordinator {
    /** The browsing mode bottom toolbar component */
    private final BrowsingModeBottomToolbarCoordinator mBrowsingModeCoordinator;

    /** The tab switcher mode bottom toolbar component */
    private TabSwitcherBottomToolbarCoordinator mTabSwitcherModeCoordinator;

    /** The tab switcher mode bottom toolbar stub that will be inflated when native is ready. */
    private final ViewStub mTabSwitcherModeStub;

    /** A provider that notifies components when the theme color changes.*/
    private final ThemeColorProvider mThemeColorProvider;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;
    private OverviewModeObserver mOverviewModeObserver;

    /** The activity tab provider. */
    private ActivityTabProvider mTabProvider;

    /**
     * Build the coordinator that manages the bottom toolbar.
     * @param stub The bottom toolbar {@link ViewStub} to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used for making the IPH.
     * @param homeButtonListener The {@link OnClickListener} for the home button.
     * @param searchAcceleratorListener The {@link OnClickListener} for the search accelerator.
     * @param shareButtonListener The {@link OnClickListener} for the share button.
     * @param themeColorProvider The {@link ThemeColorProvider} for the bottom toolbar.
     */
    BottomToolbarCoordinator(ViewStub stub, ActivityTabProvider tabProvider,
            OnClickListener homeButtonListener, OnClickListener searchAcceleratorListener,
            ObservableSupplier<OnClickListener> shareButtonListenerSupplier,
            OnLongClickListener tabsSwitcherLongClickListner,
            ThemeColorProvider themeColorProvider) {
        View root = stub.inflate();

        mBrowsingModeCoordinator = new BrowsingModeBottomToolbarCoordinator(root, tabProvider,
                homeButtonListener, searchAcceleratorListener, shareButtonListenerSupplier,
                tabsSwitcherLongClickListner);

        mTabSwitcherModeStub = root.findViewById(R.id.bottom_toolbar_tab_switcher_mode_stub);

        mThemeColorProvider = themeColorProvider;
        mTabProvider = tabProvider;
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                            tab switcher button is clicked.
     * @param newTabClickListener An {@link OnClickListener} that is triggered when the
     *                            new tab button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         menu button is clicked.
     * @param overviewModeBehavior The overview mode manager.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param topToolbarRoot The root {@link ViewGroup} of the top toolbar.
     */
    void initializeWithNative(OnClickListener tabSwitcherListener,
            OnClickListener newTabClickListener, OnClickListener closeTabsClickListener,
            AppMenuButtonHelper menuButtonHelper, OverviewModeBehavior overviewModeBehavior,
            TabCountProvider tabCountProvider, IncognitoStateProvider incognitoStateProvider,
            ViewGroup topToolbarRoot) {
        mBrowsingModeCoordinator.initializeWithNative(newTabClickListener, tabSwitcherListener,
                menuButtonHelper, tabCountProvider, mThemeColorProvider, incognitoStateProvider,
                overviewModeBehavior);
        mTabSwitcherModeCoordinator = new TabSwitcherBottomToolbarCoordinator(mTabSwitcherModeStub,
                topToolbarRoot, incognitoStateProvider, mThemeColorProvider, newTabClickListener,
                closeTabsClickListener, menuButtonHelper, tabCountProvider);

        // Do not change bottom bar if StartSurface Single Pane is enabled and HomePage is not
        // customized.
        if (!ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage()
                && BottomToolbarVariationManager.shouldBottomToolbarBeVisibleInOverviewMode()) {
            mOverviewModeBehavior = overviewModeBehavior;
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    mBrowsingModeCoordinator.getSearchAccelerator().setEnabled(false);
                    if (BottomToolbarVariationManager.isShareButtonOnBottom()) {
                        mBrowsingModeCoordinator.getShareButton().setEnabled(false);
                    }
                    if (BottomToolbarVariationManager.isHomeButtonOnBottom()) {
                        mBrowsingModeCoordinator.getHomeButton().setEnabled(false);
                    }
                }

                @Override
                public void onOverviewModeStartedHiding(
                        boolean showToolbar, boolean delayAnimation) {
                    mBrowsingModeCoordinator.getSearchAccelerator().setEnabled(true);
                    if (BottomToolbarVariationManager.isShareButtonOnBottom()) {
                        mBrowsingModeCoordinator.getShareButton().updateButtonEnabledState(
                                mTabProvider.get());
                    }
                    if (BottomToolbarVariationManager.isHomeButtonOnBottom()) {
                        mBrowsingModeCoordinator.getHomeButton().updateButtonEnabledState(
                                mTabProvider.get());
                    }
                }
            };
            if (mOverviewModeBehavior != null) {
                mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            }
        }
    }

    /**
     * @param isVisible Whether the bottom toolbar is visible.
     */
    void setBottomToolbarVisible(boolean isVisible) {
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.showToolbarOnTop(!isVisible);
        }
        mBrowsingModeCoordinator.onVisibilityChanged(isVisible);
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    void destroy() {
        mBrowsingModeCoordinator.destroy();
        if (mTabSwitcherModeCoordinator != null) {
            mTabSwitcherModeCoordinator.destroy();
            mTabSwitcherModeCoordinator = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
            mOverviewModeObserver = null;
        }
        mThemeColorProvider.destroy();
    }
}
