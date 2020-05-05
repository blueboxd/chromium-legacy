// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Shortcut/WebAPK implementation of {@link BrowserServicesActivityTabController}.
 */
@ActivityScope
public class WebappActivityTabController
        implements InflationObserver, BrowserServicesActivityTabController {
    private final Lazy<CustomTabDelegateFactory> mTabDelegateFactory;
    private final ChromeActivity<?> mActivity;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabTabPersistencePolicy mTabPersistencePolicy;
    private final CustomTabActivityTabFactory mTabFactory;
    private final ActivityTabProvider mActivityTabProvider;
    private final WebContentsFactory mWebContentsFactory;
    private final CustomTabActivityTabProvider mTabProvider;

    private HintlessActivityTabObserver mTabSwapObserver = new HintlessActivityTabObserver() {
        @Override
        public void onActivityTabChanged(@Nullable Tab tab) {
            mTabProvider.swapTab(tab);
        }
    };

    @Inject
    public WebappActivityTabController(ChromeActivity<?> activity,
            Lazy<CustomTabDelegateFactory> tabDelegateFactory,
            ActivityTabProvider activityTabProvider, TabObserverRegistrar tabObserverRegistrar,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabTabPersistencePolicy persistencePolicy, CustomTabActivityTabFactory tabFactory,
            WebContentsFactory webContentsFactory, CustomTabActivityTabProvider tabProvider) {
        mTabDelegateFactory = tabDelegateFactory;
        mActivity = activity;
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabPersistencePolicy = persistencePolicy;
        mTabFactory = tabFactory;
        mActivityTabProvider = activityTabProvider;
        mWebContentsFactory = webContentsFactory;
        mTabProvider = tabProvider;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void detachAndStartReparenting(
            Intent intent, Bundle startActivityOptions, Runnable finishCallback) {}

    @Override
    public void closeTab() {
        mTabFactory.getTabModelSelector().getCurrentModel().closeTab(
                mTabProvider.getTab(), false, false, false);
    }

    @Override
    public void closeAndForgetTab() {
        mTabFactory.getTabModelSelector().closeAllTabs(true);
        mTabPersistencePolicy.deleteMetadataStateFileAsync();
    }

    @Override
    public void saveState() {
        mTabFactory.getTabModelSelector().saveState();
    }

    @Override
    @Nullable
    public TabModelSelector getTabModelSelector() {
        return mTabFactory.getTabModelSelector();
    }

    @Override
    public void onPreInflationStartup() {
        // This must be requested before adding content.
        mActivity.supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
    }

    @Override
    public void onPostInflationStartup() {}

    public void initializeState() {
        TabModelSelectorImpl tabModelSelector = mTabFactory.getTabModelSelector();

        TabModel tabModel = tabModelSelector.getModel(false /* incognito */);
        tabModel.addObserver(mTabObserverRegistrar);

        finalizeCreatingTab(tabModelSelector, tabModel);
        Tab tab = mTabProvider.getTab();
        assert tab != null;
        assert mTabProvider.getInitialTabCreationMode() != TabCreationMode.NONE;

        // Put Sync in the correct state by calling tab state initialized. crbug.com/581811.
        tabModelSelector.markTabStateInitialized();
    }

    // Creates the tab on native init, if it hasn't been created yet, and does all the additional
    // initialization steps necessary at this stage.
    private void finalizeCreatingTab(TabModelSelectorImpl tabModelSelector, TabModel tabModel) {
        Tab tab = null;
        @TabCreationMode
        int mode = mTabProvider.getInitialTabCreationMode();

        Tab restoredTab = tryRestoringTab(tabModelSelector);
        if (restoredTab != null) {
            tab = restoredTab;
            mode = TabCreationMode.RESTORED;
        }

        if (tab == null) {
            // No tab was restored, creating a new tab.
            tab = createTab();
            mode = TabCreationMode.DEFAULT;
        }

        assert tab != null;

        if (mode != TabCreationMode.RESTORED) {
            tabModel.addTab(tab, 0, tab.getLaunchType(), TabCreationState.LIVE_IN_FOREGROUND);
        }

        mTabProvider.setInitialTab(tab, mode);

        // Listen to tab swapping and closing.
        mActivityTabProvider.addObserverAndTrigger(mTabSwapObserver);
    }

    @Nullable
    private Tab tryRestoringTab(TabModelSelectorImpl tabModelSelector) {
        if (mActivity.getSavedInstanceState() == null) return null;
        tabModelSelector.loadState(true);
        tabModelSelector.restoreTabs(true);
        Tab tab = tabModelSelector.getCurrentTab();
        if (tab != null) {
            initializeTab(tab);
        }
        return tab;
    }

    private Tab createTab() {
        WebContents webContents =
                mWebContentsFactory.createWebContentsWithWarmRenderer(false /* incognito */, false);
        Tab tab = mTabFactory.createTab(webContents, mTabDelegateFactory.get(), null);
        initializeTab(tab);
        return tab;
    }

    private void initializeTab(Tab tab) {
        mTabObserverRegistrar.addObserversForTab(tab);
    }
}
