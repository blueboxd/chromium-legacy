// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.customtabs.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;

import javax.inject.Inject;
import javax.inject.Named;

import dagger.Lazy;

/**
 * Works with the toolbar in a Custom Tab. Encapsulates interactions with Chrome's toolbar-related
 * classes such as {@link ToolbarManager} and {@link FullscreenManager}.
 *
 * TODO(pshmakov):
 * 1. Reduce the coupling between Custom Tab toolbar and Chrome's common code. In particular,
 * ToolbarLayout has Custom Tab specific methods that throw unless we're in a Custom Tab - we need a
 * better design.
 * 2. Make toolbar lazy. E.g. in Trusted Web Activities we always start without toolbar - delay
 * executing any initialization code and creating {@link ToolbarManager} until the toolbar needs
 * to appear.
 * 3. Refactor to MVC.
 */
@ActivityScope
public class CustomTabToolbarCoordinator implements InflationObserver, NativeInitObserver {

    private final Lazy<ToolbarManager> mToolbarManager;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabsConnection mConnection;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final ChromeActivity mActivity;
    private final Context mAppContext;
    private final CustomTabActivityTabController mTabController;
    private final Lazy<ChromeFullscreenManager> mFullscreenManager;
    private final CustomTabActivityNavigationController mNavigationController;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private final CustomTabBrowserControlsVisibilityDelegate mVisibilityDelegate;

    private int mControlsHidingToken = FullscreenManager.INVALID_TOKEN;
    private boolean mInitializedToolbarWithNative;

    @Inject
    public CustomTabToolbarCoordinator(ActivityLifecycleDispatcher lifecycleDispatcher,
            Lazy<ToolbarManager> toolbarManager,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider tabProvider,
            CustomTabsConnection connection,
            Lazy<CompositorViewHolder> compositorViewHolder,
            ChromeActivity activity,
            @Named(APP_CONTEXT) Context appContext,
            CustomTabActivityTabController tabController,
            Lazy<ChromeFullscreenManager> fullscreenManager,
            CustomTabActivityNavigationController navigationController,
            TabObserverRegistrar tabObserverRegistrar,
            CustomTabStatusBarColorProvider statusBarColorProvider,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate) {
        mToolbarManager = toolbarManager;
        mIntentDataProvider = intentDataProvider;
        mTabProvider = tabProvider;
        mConnection = connection;
        mCompositorViewHolder = compositorViewHolder;
        mActivity = activity;
        mAppContext = appContext;
        mTabController = tabController;
        mFullscreenManager = fullscreenManager;
        mNavigationController = navigationController;
        mTabObserverRegistrar = tabObserverRegistrar;
        mStatusBarColorProvider = statusBarColorProvider;
        mVisibilityDelegate = visibilityDelegate;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        ToolbarManager manager = mToolbarManager.get();
        assert manager != null : "Toolbar manager not initialized";

        manager.setCloseButtonDrawable(FeatureUtilities.isNoTouchModeEnabled()
                ? null
                : mIntentDataProvider.getCloseButtonDrawable());
        manager.setShowTitle(
                mIntentDataProvider.getTitleVisibilityState() == CustomTabsIntent.SHOW_PAGE_TITLE);
        if (mConnection.shouldHideDomainForSession(mIntentDataProvider.getSession())) {
            manager.setUrlBarHidden(true);
        }
        int toolbarColor = mIntentDataProvider.getToolbarColor();
        manager.onThemeColorChanged(toolbarColor, false);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            manager.setShouldUpdateToolbarPrimaryColor(false);
        }
        if (mIntentDataProvider.isMediaViewer()) {
            manager.setToolbarShadowVisibility(View.GONE);
        }
        showCustomButtonsOnToolbar();
        observeTabToUpdateColor();
    }

    /**
     * Configures the custom button on toolbar. Does nothing if invalid data is provided by clients.
     */
    private void showCustomButtonsOnToolbar() {
        for (CustomButtonParams params : mIntentDataProvider.getCustomButtonsOnToolbar()) {
            View.OnClickListener onClickListener = v -> onCustomButtonClick(params);
            mToolbarManager.get().addCustomActionButton(params.getIcon(mActivity),
                    params.getDescription(), onClickListener);
        }
    }

    private void onCustomButtonClick(CustomButtonParams params) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return;
        mIntentDataProvider.sendButtonPendingIntentWithUrlAndTitle(mAppContext, params,
                tab.getUrl(), tab.getTitle());
        RecordUserAction.record("CustomTabsCustomActionButtonClick");

        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()
                && TextUtils.equals(params.getDescription(), mActivity.getString(R.string.share))) {
            RecordUserAction.record("CustomTabsCustomActionButtonClick.DownloadsUI.Share");
        }
    }

    private void observeTabToUpdateColor() {
        mTabObserverRegistrar.registerTabObserver(new EmptyTabObserver() {
            /** Keeps track of the original color before the preview was shown. */
            private int mOriginalColor;

            /** True if a change to the toolbar color was made because of a preview. */
            private boolean mTriggeredPreviewChange;

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Update the color when the page load finishes.
                updateColor(tab);
            }

            @Override
            public void didReloadLoFiImages(Tab tab) {
                // Update the color when the LoFi preview is reloaded.
                updateColor(tab);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the color on every new URL.
                updateColor(tab);
            }

            /**
             * Updates the color of the Activity's status bar and the CCT Toolbar. When a preview is
             * shown, it should be reset to the default color. If the user later navigates away from
             * that preview to a non-preview page, reset the color back to the original. This does
             * not interfere with site-specific theme colors which are disabled when a preview is
             * being shown.
             */
            private void updateColor(Tab tab) {
                ToolbarManager manager = mToolbarManager.get();

                // Record the original toolbar color in case we need to revert back to it later
                // after a preview has been shown then the user navigates to another non-preview
                // page.
                if (mOriginalColor == 0) mOriginalColor = manager.getPrimaryColor();

                final boolean shouldUpdateOriginal = manager.getShouldUpdateToolbarPrimaryColor();
                manager.setShouldUpdateToolbarPrimaryColor(true);

                if (tab.isPreview()) {
                    final int defaultColor = ColorUtils.getDefaultThemeColor(
                            mActivity.getResources(), false);
                    manager.onThemeColorChanged(defaultColor, false);
                    mTriggeredPreviewChange = true;
                } else if (mOriginalColor != manager.getPrimaryColor() && mTriggeredPreviewChange) {
                    manager.onThemeColorChanged(mOriginalColor, false);
                    mTriggeredPreviewChange = false;
                    mOriginalColor = 0;
                }

                mStatusBarColorProvider.onToolbarColorChanged();
                manager.setShouldUpdateToolbarPrimaryColor(shouldUpdateOriginal);
            }
        });
    }

    @Override
    public void onFinishNativeInitialization() {
        LayoutManager layoutDriver = new LayoutManager(mCompositorViewHolder.get());
        mActivity.initializeCompositorContent(layoutDriver,
                mActivity.findViewById(R.id.url_bar),
                mActivity.findViewById(android.R.id.content),
                mActivity.findViewById(R.id.control_container));

        mToolbarManager.get().initializeWithNative(mTabController.getTabModelSelector(),
                mFullscreenManager.get().getBrowserVisibilityDelegate(), null, layoutDriver, null,
                null, null, v -> onCloseButtonClick());
        mInitializedToolbarWithNative = true;
    }

    private void onCloseButtonClick() {
        RecordUserAction.record("CustomTabs.CloseButtonClicked");
        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
            RecordUserAction.record("CustomTabs.CloseButtonClicked.DownloadsUI");
        }
        mNavigationController.navigateOnClose();
    }

    /**
     * Sets the hidden state. If set to true, toolbar stays hidden. If set to false, toolbar
     * behaves normally: appears and disappears while scrolling, etc.
     */
    public void setToolbarHidden(boolean hidden) {
        mVisibilityDelegate.setControlsHidden(hidden);
        if (hidden) {
            mControlsHidingToken = mFullscreenManager.get()
                    .hideAndroidControlsAndClearOldToken(mControlsHidingToken);
        } else {
            mFullscreenManager.get().releaseAndroidControlsHidingToken(mControlsHidingToken);
        }
    }

    /**
     * Shows toolbar temporarily, for a few seconds.
     */
    public void showToolbarTemporarily() {
        mFullscreenManager.get().getBrowserVisibilityDelegate().showControlsTransient();
    }

    /**
     * Updates a custom button with a new icon and description. Provided {@link CustomButtonParams}
     * must have the updated data.
     * Returns whether has succeeded.
     */
    public boolean updateCustomButton(CustomButtonParams params) {
        if (!params.doesIconFitToolbar(mActivity)) {
            return false;
        }
        int index = mIntentDataProvider.getCustomToolbarButtonIndexForId(params.getId());
        if (index == -1) {
            assert false;
            return false;
        }
        mToolbarManager.get().updateCustomActionButton(index, params.getIcon(mActivity),
                params.getDescription());
        return true;
    }

    /**
     * Returns whether the toolbar has been fully initialized
     * ({@link ToolbarManager#initializeWithNative}).
     */
    public boolean toolbarIsInitialized() {
        return mInitializedToolbarWithNative;
    }
}
