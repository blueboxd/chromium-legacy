// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

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

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final Callback<ShareDelegate> mShareDelegateSupplierCallback;
    private ObservableSupplierImpl<OnClickListener> mShareButtonListenerSupplier =
            new ObservableSupplierImpl<>();
    private final Supplier<Boolean> mShowStartSurfaceCallable;

    /**
     * Build the coordinator that manages the bottom toolbar.
     * @param stub The bottom toolbar {@link ViewStub} to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used for making the IPH.
     * @param themeColorProvider The {@link ThemeColorProvider} for the bottom toolbar.
     * @param shareDelegateSupplier The supplier for the {@link ShareDelegate} the bottom controls
     *         should use to share content.
     * @param showStartSurfaceCallable The action that opens the start surface, returning true if
     * the start surface is shown.
     * @param openHomepageAction The action that opens the homepage.
     * @param setUrlBarFocusAction The function that sets Url bar focus. The first argument is
     */
    BottomToolbarCoordinator(ViewStub stub, ActivityTabProvider tabProvider,
            OnLongClickListener tabsSwitcherLongClickListner, ThemeColorProvider themeColorProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Boolean> showStartSurfaceCallable, Runnable openHomepageAction,
            Callback<Integer> setUrlBarFocusAction) {
        View root = stub.inflate();

        mShowStartSurfaceCallable = showStartSurfaceCallable;
        final OnClickListener homeButtonListener = v -> {
            recordBottomToolbarUseForIPH();
            openHomepageAction.run();
        };

        final OnClickListener searchAcceleratorListener = v -> {
            recordBottomToolbarUseForIPH();
            RecordUserAction.record("MobileToolbarOmniboxAcceleratorTap");

            // Only switch to HomePage when overview is showing.
            if (mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible()) {
                mShowStartSurfaceCallable.get();
            }
            setUrlBarFocusAction.onResult(LocationBar.OmniboxFocusReason.ACCELERATOR_TAP);
        };

        mBrowsingModeCoordinator = new BrowsingModeBottomToolbarCoordinator(root, tabProvider,
                homeButtonListener, searchAcceleratorListener, mShareButtonListenerSupplier,
                tabsSwitcherLongClickListner);

        mTabSwitcherModeStub = root.findViewById(R.id.bottom_toolbar_tab_switcher_mode_stub);

        mThemeColorProvider = themeColorProvider;
        mTabProvider = tabProvider;

        mShareDelegateSupplier = shareDelegateSupplier;
        mShareDelegateSupplierCallback = this::onShareDelegateAvailable;
        mShareDelegateSupplier.addObserver(mShareDelegateSupplierCallback);
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
     * @param closeAllTabsAction The runnable that closes all tabs in the current tab model.
     */
    void initializeWithNative(OnClickListener tabSwitcherListener,
            OnClickListener newTabClickListener, AppMenuButtonHelper menuButtonHelper,
            OverviewModeBehavior overviewModeBehavior, TabCountProvider tabCountProvider,
            IncognitoStateProvider incognitoStateProvider, ViewGroup topToolbarRoot,
            Runnable closeAllTabsAction) {
        final OnClickListener closeTabsClickListener = v -> {
            recordBottomToolbarUseForIPH();
            final boolean isIncognito = incognitoStateProvider.isIncognitoSelected();
            if (isIncognito) {
                RecordUserAction.record("MobileToolbarCloseAllIncognitoTabsButtonTap");
            } else {
                RecordUserAction.record("MobileToolbarCloseAllRegularTabsButtonTap");
            }

            closeAllTabsAction.run();
        };

        if (menuButtonHelper != null) {
            menuButtonHelper.setOnClickRunnable(() -> recordBottomToolbarUseForIPH());
        }

        newTabClickListener = wrapBottomToolbarClickListenerForIPH(newTabClickListener);
        tabSwitcherListener = wrapBottomToolbarClickListenerForIPH(tabSwitcherListener);
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
        mShareDelegateSupplier.removeObserver(mShareDelegateSupplierCallback);
    }

    private void onShareDelegateAvailable(ShareDelegate shareDelegate) {
        final OnClickListener shareButtonListener = v -> {
            if (BottomToolbarVariationManager.isShareButtonOnBottom()) {
                recordBottomToolbarUseForIPH();
                RecordUserAction.record("MobileBottomToolbarShareButton");
            }

            Tab tab = mTabProvider.get();
            shareDelegate.share(tab, /*shareDirectly=*/false);
        };

        mShareButtonListenerSupplier.set(shareButtonListener);
    }

    /** Record that the bottom toolbar was used for IPH reasons. */
    private void recordBottomToolbarUseForIPH() {
        Tab tab = mTabProvider.get();
        if (tab == null) return;

        Tracker tracker =
                TrackerFactory.getTrackerForProfile(Profile.fromWebContents(tab.getWebContents()));
        tracker.notifyEvent(EventConstants.CHROME_DUET_USED_BOTTOM_TOOLBAR);
    }

    /**
     * Add bottom toolbar IPH tracking to an existing click listener.
     * @param listener The listener to add bottom toolbar tracking to.
     */
    private OnClickListener wrapBottomToolbarClickListenerForIPH(OnClickListener listener) {
        return (v) -> {
            recordBottomToolbarUseForIPH();
            listener.onClick(v);
        };
    }
}
