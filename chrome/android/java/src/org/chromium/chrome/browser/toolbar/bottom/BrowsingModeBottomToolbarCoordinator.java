// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonCoordinator;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonView;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the browsing mode bottom toolbar. This class has two primary components,
 * an Android view that handles user actions and a composited texture that draws when the controls
 * are being scrolled off-screen. The Android version does not draw unless the controls offset is 0.
 */
public class BrowsingModeBottomToolbarCoordinator {
    /** The mediator that handles events from outside the browsing mode bottom toolbar. */
    private final BrowsingModeBottomToolbarMediator mMediator;

    /** The home button that lives in the bottom toolbar. */
    private final HomeButton mHomeButton;

    /** The share button that lives in the bottom toolbar. */
    private final ShareButton mShareButton;

    /** The search accelerator that lives in the bottom toolbar. */
    private final SearchAccelerator mSearchAccelerator;

    /** The tab switcher button component that lives in the bottom toolbar. */
    private final TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;

    /** The tab switcher button view that lives in the bottom toolbar. */
    private final TabSwitcherButtonView mTabSwitcherButtonView;

    /** The menu button that lives in the browsing mode bottom toolbar. */
    private final MenuButton mMenuButton;

    /** The view group that includes all views shown on browsing mode */
    private final BrowsingModeBottomToolbarLinearLayout mToolbarRoot;

    /** The model for the browsing mode bottom toolbar that holds all of its state. */
    private final BrowsingModeBottomToolbarModel mModel;

    /** The callback to be exectured when the share button on click listener is available. */
    private final Callback<OnClickListener> mShareButtonListenerSupplierCallback;

    /** The supplier for the share button on click listener. */
    private final ObservableSupplier<OnClickListener> mShareButtonListenerSupplier;

    /** The activity tab provider that used for making the IPH. */
    private final ActivityTabProvider mTabProvider;

    /**
     * Build the coordinator that manages the browsing mode bottom toolbar.
     * @param root The root {@link View} for locating the views to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used for making the IPH.
     * @param homeButtonListener The {@link OnClickListener} for the home button.
     * @param searchAcceleratorListener The {@link OnClickListener} for the search accelerator.
     * @param shareButtonListener The {@link OnClickListener} for the share button.
     */
    BrowsingModeBottomToolbarCoordinator(View root, ActivityTabProvider tabProvider,
            OnClickListener homeButtonListener, OnClickListener searchAcceleratorListener,
            ObservableSupplier<OnClickListener> shareButtonListenerSupplier,
            OnLongClickListener tabSwitcherLongClickListner) {
        mModel = new BrowsingModeBottomToolbarModel();
        mToolbarRoot = root.findViewById(R.id.bottom_toolbar_browsing);
        mTabProvider = tabProvider;

        PropertyModelChangeProcessor.create(
                mModel, mToolbarRoot, new BrowsingModeBottomToolbarViewBinder());

        mMediator = new BrowsingModeBottomToolbarMediator(mModel);

        mHomeButton = mToolbarRoot.findViewById(R.id.home_button);
        mHomeButton.setWrapperView(mToolbarRoot.findViewById(R.id.home_button_wrapper));
        mHomeButton.setOnClickListener(homeButtonListener);
        mHomeButton.setActivityTabProvider(mTabProvider);
        setupIPH(FeatureConstants.CHROME_DUET_HOME_BUTTON_FEATURE, mHomeButton, homeButtonListener);

        mShareButton = mToolbarRoot.findViewById(R.id.share_button);
        mShareButton.setWrapperView(mToolbarRoot.findViewById(R.id.share_button_wrapper));
        mShareButtonListenerSupplierCallback = shareButtonListener -> {
            mShareButton.setOnClickListener(shareButtonListener);
        };
        mShareButtonListenerSupplier = shareButtonListenerSupplier;
        mShareButtonListenerSupplier.addObserver(mShareButtonListenerSupplierCallback);
        mShareButton.setActivityTabProvider(mTabProvider);

        mSearchAccelerator = mToolbarRoot.findViewById(R.id.search_accelerator);
        mSearchAccelerator.setWrapperView(
                mToolbarRoot.findViewById(R.id.search_accelerator_wrapper));
        mSearchAccelerator.setOnClickListener(searchAcceleratorListener);
        setupIPH(FeatureConstants.CHROME_DUET_SEARCH_FEATURE, mSearchAccelerator,
                searchAcceleratorListener);

        mTabSwitcherButtonCoordinator = new TabSwitcherButtonCoordinator(mToolbarRoot);
        // TODO(amaralp): Make this adhere to MVC framework.
        mTabSwitcherButtonView = mToolbarRoot.findViewById(R.id.tab_switcher_button);
        mTabSwitcherButtonView.setWrapperView(
                mToolbarRoot.findViewById(R.id.tab_switcher_button_wrapper));

        // TODO(lazzzis): Refactor the long click listener. Have to specify the handler view here
        //      in order to fix the anchor view of the long-tap menu
        mTabSwitcherButtonView.setOnLongClickListener(
                (view) -> tabSwitcherLongClickListner.onLongClick(mTabSwitcherButtonView));
        mMenuButton = mToolbarRoot.findViewById(R.id.menu_button_wrapper);
        mMenuButton.setWrapperView(mToolbarRoot.findViewById(R.id.labeled_menu_button_wrapper));
    }

    /**
     * Setup and show the IPH bubble for Chrome Duet if needed.
     * @param feature A String identifying the feature.
     * @param anchor The view to anchor the IPH to.
     * @param listener An {@link OnClickListener} that is triggered when IPH is clicked. {@link
     *         HomeButton} and {@link SearchAccelerator} need to pass this parameter, {@link
     *         TabSwitcherButtonView} just passes null.
     */
    void setupIPH(@FeatureConstants String feature, View anchor, OnClickListener listener) {
        mTabProvider.addObserverAndTrigger(new HintlessActivityTabObserver() {
            @Override
            public void onActivityTabChanged(Tab tab) {
                if (tab == null) return;
                TabImpl tabImpl = (TabImpl) tab;
                final Tracker tracker = TrackerFactory.getTrackerForProfile(tabImpl.getProfile());
                final Runnable completeRunnable = () -> {
                    if (listener != null) {
                        listener.onClick(anchor);
                    }
                };
                tracker.addOnInitializedCallback((ready) -> {
                    mMediator.showIPH(
                            feature, tabImpl.getActivity(), anchor, tracker, completeRunnable);
                });
                mTabProvider.removeObserver(this);
            }
        });
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                            tab switcher button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         menu button is clicked.
     * @param tabCountProvider Updates the tab count number in the tab switcher button.
     * @param themeColorProvider Notifies components when theme color changes.
     * @param incognitoStateProvider Notifies components when incognito state changes.
     */
    void initializeWithNative(OnClickListener tabSwitcherListener,
            AppMenuButtonHelper menuButtonHelper, TabCountProvider tabCountProvider,
            ThemeColorProvider themeColorProvider, IncognitoStateProvider incognitoStateProvider) {
        mMediator.setThemeColorProvider(themeColorProvider);

        mHomeButton.setThemeColorProvider(themeColorProvider);
        mShareButton.setThemeColorProvider(themeColorProvider);
        mSearchAccelerator.setThemeColorProvider(themeColorProvider);
        mSearchAccelerator.setIncognitoStateProvider(incognitoStateProvider);

        mTabSwitcherButtonCoordinator.setTabSwitcherListener(tabSwitcherListener);
        mTabSwitcherButtonCoordinator.setThemeColorProvider(themeColorProvider);
        mTabSwitcherButtonCoordinator.setTabCountProvider(tabCountProvider);
        // Send null to IPH here to avoid tabSwitcherListener to be called twince, since
        // mTabSwitcherButtonView has it own OnClickListener, but other buttons set OnClickListener
        // to their wrappers.
        setupIPH(FeatureConstants.CHROME_DUET_TAB_SWITCHER_FEATURE, mTabSwitcherButtonView, null);

        assert menuButtonHelper != null;
        mMenuButton.setAppMenuButtonHelper(menuButtonHelper);
        mMenuButton.setThemeColorProvider(themeColorProvider);
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    void showAppMenuUpdateBadge() {
        mMenuButton.showAppMenuUpdateBadgeIfAvailable(true);
    }

    /**
     * Remove the update badge.
     */
    void removeAppMenuUpdateBadge() {
        mMenuButton.removeAppMenuUpdateBadge(true);
    }

    /**
     * @return Whether the update badge is showing.
     */
    boolean isShowingAppMenuUpdateBadge() {
        return mMenuButton.isShowingAppMenuUpdateBadge();
    }

    /**
     * @param enabled Whether to disable click events on the bottom toolbar. Setting true can also
     *                prevent from all click events on toolbar and all children views on toolbar.
     */
    void setTouchEnabled(boolean enabled) {
        mToolbarRoot.setTouchEnabled(enabled);
    }

    /**
     * @param visible Whether to hide the tab switcher bottom toolbar
     */
    void setVisible(boolean visible) {
        mModel.set(BrowsingModeBottomToolbarModel.IS_VISIBLE, visible);
    }

    /**
     * @return The browsing mode bottom toolbar's menu button.
     */
    MenuButton getMenuButton() {
        return mMenuButton;
    }

    /**
     * @return The browsing mode bottom toolbar's share button.
     */
    ShareButton getShareButton() {
        return mShareButton;
    }

    /**
     * @return The browsing mode bottom toolbar's tab switcher button.
     */
    TabSwitcherButtonView getTabSwitcherButtonView() {
        return mTabSwitcherButtonView;
    }

    /**
     * @return The browsing mode bottom toolbar's search button.
     */
    SearchAccelerator getSearchAccelerator() {
        return mSearchAccelerator;
    }

    /**
     * @return The browsing mode bottom toolbar's home button.
     */
    HomeButton getHomeButton() {
        return mHomeButton;
    }

    /**
     * Clean up any state when the browsing mode bottom toolbar is destroyed.
     */
    public void destroy() {
        mShareButtonListenerSupplier.removeObserver(mShareButtonListenerSupplierCallback);
        mMediator.destroy();
        mHomeButton.destroy();
        mShareButton.destroy();
        mSearchAccelerator.destroy();
        mTabSwitcherButtonCoordinator.destroy();
        mMenuButton.destroy();
    }
}
