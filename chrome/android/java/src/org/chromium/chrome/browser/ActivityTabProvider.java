// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

/**
 * A class that provides the current {@link Tab} for various states of the browser's activity.
 */
public class ActivityTabProvider extends ObservableSupplierImpl<Tab> {
    /**
     * A utility class for observing the activity tab via {@link TabObserver}. When the activity
     * tab changes, the observer is switched to that tab.
     */
    public static class ActivityTabTabObserver extends EmptyTabObserver {
        /** A handle to the activity tab provider. */
        private final ActivityTabProvider mTabProvider;

        /** An observer to watch for a changing activity tab and move this tab observer. */
        private final Callback<Tab> mActivityTabObserver;

        /** The current activity tab. */
        private Tab mTab;

        /**
         * Create a new {@link TabObserver} that only observes the activity tab.
         * @param tabProvider An {@link ActivityTabProvider} to get the activity tab.
         */
        public ActivityTabTabObserver(ActivityTabProvider tabProvider) {
            mTabProvider = tabProvider;
            mActivityTabObserver = (tab) -> {
                updateObservedTab(tab);
                onObservingDifferentTab(tab);
            };
            addObserverToTabProvider();
            updateObservedTabToCurrent();
        }

        /**
         * Update the tab being observed.
         * @param newTab The new tab to observe.
         */
        private void updateObservedTab(Tab newTab) {
            if (mTab != null) mTab.removeObserver(ActivityTabTabObserver.this);
            mTab = newTab;
            if (mTab != null) mTab.addObserver(ActivityTabTabObserver.this);
        }

        /**
         * A notification that the observer has switched to observing a different tab. This will not
         * be called for the initial tab being attached to after creation.
         * @param tab The tab that the observer is now observing. This can be null.
         */
        protected void onObservingDifferentTab(Tab tab) {}

        /**
         * Clean up any state held by this observer.
         */
        @CallSuper
        public void destroy() {
            if (mTab != null) {
                mTab.removeObserver(this);
                mTab = null;
            }
            removeObserverFromTabProvider();
        }

        @VisibleForTesting
        protected void updateObservedTabToCurrent() {
            updateObservedTab(mTabProvider.get());
        }

        @VisibleForTesting
        protected void addObserverToTabProvider() {
            mTabProvider.addObserver(mActivityTabObserver);
        }

        @VisibleForTesting
        protected void removeObserverFromTabProvider() {
            mTabProvider.removeObserver(mActivityTabObserver);
        }
    }

    /** A handle to the {@link LayoutManager} to get the active layout. */
    private LayoutManager mLayoutManager;

    /** The observer watching scene changes in the {@link LayoutManager}. */
    private SceneChangeObserver mSceneChangeObserver;

    /** A handle to the {@link TabModelSelector}. */
    private TabModelSelector mTabModelSelector;

    /** An observer for watching tab creation and switching events. */
    private TabModelSelectorTabModelObserver mTabModelObserver;

    /** Default constructor. */
    public ActivityTabProvider() {
        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {}

            @Override
            public void onSceneChange(Layout layout) {
                // The {@link SimpleAnimationLayout} is a special case, the intent is not to switch
                // tabs, but to merely run an animation. In this case, do nothing. If the animation
                // layout does result in a new tab {@link TabModelObserver#didSelectTab} will
                // trigger the event instead. If the tab does not change, the event will no
                if (layout instanceof SimpleAnimationLayout) return;

                Tab tab = mTabModelSelector.getCurrentTab();
                if (!(layout instanceof StaticLayout)) tab = null;
                triggerActivityTabChangeEvent(tab);
            }
        };
    }

    /**
     * @param selector A {@link TabModelSelector} for watching for changes in tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        assert mTabModelSelector == null;
        mTabModelSelector = selector;
        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                triggerActivityTabChangeEvent(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                // If this is the last tab to close, make sure a signal is sent to the observers.
                if (mTabModelSelector.getTotalTabCount() <= 1) triggerActivityTabChangeEvent(null);
            }
        };
    }

    /**
     * @param layoutManager A {@link LayoutManager} for watching for scene changes.
     */
    public void setLayoutManager(LayoutManager layoutManager) {
        assert mLayoutManager == null;
        mLayoutManager = layoutManager;
        mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
    }

    /**
     * Check if the interactive tab change event needs to be triggered based on the provided tab.
     * @param tab The activity's tab.
     */
    private void triggerActivityTabChangeEvent(Tab tab) {
        // Allow the event to trigger before native is ready (before the layout manager is set).
        if (mLayoutManager != null
                && !(mLayoutManager.getActiveLayout() instanceof StaticLayout
                        || mLayoutManager.getActiveLayout() instanceof SimpleAnimationLayout)
                && tab != null) {
            return;
        }

        set(tab);
    }

    /** Clean up and detach any observers this object created. */
    public void destroy() {
        if (mLayoutManager != null) mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
        mLayoutManager = null;
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        mTabModelSelector = null;
    }
}
