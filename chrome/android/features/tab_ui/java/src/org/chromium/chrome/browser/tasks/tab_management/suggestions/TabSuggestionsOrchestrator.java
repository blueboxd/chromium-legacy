// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Represents the entry point for the TabSuggestions component. Responsible for
 * registering and invoking the different {@link TabSuggestionsFetcher}.
 */
public class TabSuggestionsOrchestrator implements TabSuggestions, Destroyable {
    public static final String TAB_SUGGESTIONS_UMA_PREFIX = "TabSuggestionsOrchestrator";
    private static final String TAG = "TabSuggestionsDetailed";
    private static final int MIN_CLOSE_SUGGESTIONS_THRESHOLD = 3;

    protected TabContextObserver mTabContextObserver;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private List<TabSuggestionsFetcher> mTabSuggestionsFetchers;
    private List<TabSuggestion> mPrefetchedResults = new LinkedList<>();
    private TabContext mPrefetchedTabContext;
    private TabModelSelector mTabModelSelector;

    public TabSuggestionsOrchestrator(
            TabModelSelector selector, ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mTabModelSelector = selector;
        mTabSuggestionsFetchers = new LinkedList<>();
        mTabSuggestionsFetchers.add(new TabSuggestionsClientFetcher());
        mTabContextObserver = new TabContextObserver(selector) {
            @Override
            public void onTabContextChanged(@TabContextChangeReason int changeReason) {
                prefetchSuggestions();
            }
        };
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(this);
    }

    @Override
    public List<TabSuggestion> getSuggestions(TabContext tabContext) {
        synchronized (mPrefetchedResults) {
            if (tabContext.equals(mPrefetchedTabContext)) {
                return aggregateResults(mPrefetchedResults);
            }
            return new LinkedList<>();
        }
    }

    private List<TabSuggestion> aggregateResults(List<TabSuggestion> tabSuggestions) {
        List<TabSuggestion> aggregated = new LinkedList<>();
        for (TabSuggestion tabSuggestion : tabSuggestions) {
            switch (tabSuggestion.getAction()) {
                case TabSuggestion.TabSuggestionAction.CLOSE:
                    if (tabSuggestion.getTabsInfo().size() >= MIN_CLOSE_SUGGESTIONS_THRESHOLD) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                case TabSuggestion.TabSuggestionAction.GROUP:
                    if (!tabSuggestion.getTabsInfo().isEmpty()) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                default:
                    android.util.Log.e(
                            TAG, String.format("Unknown action: %d", tabSuggestion.getAction()));
                    break;
            }
        }
        Collections.shuffle(aggregated);
        return aggregated;
    }

    @Override
    public void destroy() {
        mTabContextObserver.destroy();
        mActivityLifecycleDispatcher.unregister(this);
    }

    /**
     * Acquire suggestions and store so suggestions are available for the UI
     * thread on demand.
     */
    protected void prefetchSuggestions() {
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        synchronized (mPrefetchedResults) {
            mPrefetchedTabContext = tabContext;
            mPrefetchedResults = new LinkedList<>();
            for (TabSuggestionsFetcher tabSuggestionsFetcher : mTabSuggestionsFetchers) {
                if (tabSuggestionsFetcher.isEnabled()) {
                    tabSuggestionsFetcher.fetch(tabContext, res -> prefetchCallback(res));
                }
            }
        }
    }

    private void prefetchCallback(TabSuggestionsFetcherResults suggestions) {
        synchronized (mPrefetchedResults) {
            if (suggestions.tabContext.equals(mPrefetchedTabContext)) {
                mPrefetchedResults.addAll(suggestions.tabSuggestions);
            }
        }
    }
}
