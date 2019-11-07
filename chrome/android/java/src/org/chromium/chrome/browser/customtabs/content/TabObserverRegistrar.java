// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;

/**
 * Adds and removes the given {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to Tabs as
 * they enter/leave the TabModel.
 *
 * // TODO(peconn): Get rid of EmptyTabModelObserver now that we have Java 8 default methods.
 */
@ActivityScope
public class TabObserverRegistrar extends EmptyTabModelObserver implements Destroyable {
    private final Set<PageLoadMetrics.Observer> mPageLoadMetricsObservers = new HashSet<>();
    private final Set<TabObserver> mTabObservers = new HashSet<>();

    /**
     * Registers a {@link PageLoadMetrics.Observer} to be managed by this Registrar.
     */
    public void registerPageLoadMetricsObserver(PageLoadMetrics.Observer observer) {
        mPageLoadMetricsObservers.add(observer);
    }

    /**
     * Registers a {@link TabObserver} to be managed by this Registrar.
     */
    public void registerTabObserver(TabObserver observer) {
        mTabObservers.add(observer);
    }

    /**
     * Unregisters a {@link TabObserver} to be managed by this Registrar.
     */
    public void unregisterTabObserver(TabObserver observer) {
        mTabObservers.remove(observer);
    }

    @Inject
    public TabObserverRegistrar(ActivityLifecycleDispatcher lifecycleDispatcher) {
        lifecycleDispatcher.register(this);
    }

    @Override
    public void didAddTab(Tab tab, int type) {
        addObserversForTab(tab);
    }

    @Override
    public void didCloseTab(int tabId, boolean incognito) {
        // We don't need to remove the Tab Observers since it's closed.
        // TODO(peconn): Do we really want to remove the *global* PageLoadMetrics observers here?
        removePageLoadMetricsObservers();
    }

    @Override
    public void tabRemoved(Tab tab) {
        removePageLoadMetricsObservers();
        removeTabObservers(tab);
    }

    /**
     * Adds all currently registered {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to
     * the global {@link PageLoadMetrics} object and the given {@link Tab} respectively.
     */
    public void addObserversForTab(Tab tab) {
        addPageLoadMetricsObservers();
        addTabObservers(tab);
    }

    private void addPageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.addObserver(observer);
        }
    }

    private void removePageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.removeObserver(observer);
        }
    }

    private void addTabObservers(Tab tab) {
        for (TabObserver observer : mTabObservers) {
            tab.addObserver(observer);
        }
    }

    private void removeTabObservers(Tab tab) {
        for (TabObserver observer : mTabObservers) {
            tab.removeObserver(observer);
        }
    }

    @Override
    public void destroy() {
        removePageLoadMetricsObservers();
    }
}
