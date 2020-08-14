// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Data that will be used later when a tab is opened via an intent. Often only the necessary
 * subset of the data will be set. All data is removed once the tab finishes initializing.
 */
public class AsyncTabParamsManager {
    /** Singleton instance. */
    private static final AsyncTabParamsManager INSTANCE = new AsyncTabParamsManager();

    /** A map of tab IDs to AsyncTabParams consumed by Activities started asynchronously. */
    private final SparseArray<AsyncTabParams> mAsyncTabParams = new SparseArray<>();

    private boolean mAddedToIncognitoTabHostRegistry;

    /** Get the singleton instance of {@link AsyncTabParamsManager}. */
    public static AsyncTabParamsManager getInstance() {
        return INSTANCE;
    }

    /**
     * Stores AsyncTabParams used when the tab with the given ID is launched via intent.
     * @param tabId The ID of the tab that will be launched via intent.
     * @param params The AsyncTabParams to use when creating the Tab.
     */
    public void add(int tabId, AsyncTabParams params) {
        mAsyncTabParams.put(tabId, params);

        if (!mAddedToIncognitoTabHostRegistry) {
            // Make sure async incognito tabs are taken into account when, for example,
            // checking if any incognito tabs exist.
            IncognitoTabHostRegistry.getInstance().register(new AsyncTabsIncognitoTabHost(this));
            mAddedToIncognitoTabHostRegistry = true;
        }
    }

    /**
     * @return Whether there is already an {@link AsyncTabParams} added for the given ID.
     */
    public boolean hasParamsForTabId(int tabId) {
        return mAsyncTabParams.get(tabId) != null;
    }

    /**
     * @return Whether there are any saved {@link AsyncTabParams} with a tab to reparent. All
     *         implementations of this are keyed off of a user gesture so the likelihood of having
     *         more than one is zero.
     */
    public boolean hasParamsWithTabToReparent() {
        for (int i = 0; i < mAsyncTabParams.size(); i++) {
            if (mAsyncTabParams.get(mAsyncTabParams.keyAt(i)).getTabToReparent() == null) continue;
            return true;
        }
        return false;
    }

    /**
     * @return A map of tab IDs to AsyncTabParams containing data that will be used later when a tab
     *         is opened via an intent.
     */
    public SparseArray<AsyncTabParams> getAsyncTabParams() {
        return mAsyncTabParams;
    }

    /**
     * @return Retrieves and removes AsyncTabCreationParams for a particular tab id.
     */
    public AsyncTabParams remove(int tabId) {
        AsyncTabParams data = mAsyncTabParams.get(tabId);
        mAsyncTabParams.remove(tabId);
        return data;
    }

    private AsyncTabParamsManager() {}

    private static class AsyncTabsIncognitoTabHost implements IncognitoTabHost {
        private final AsyncTabParamsManager mAsyncTabParamsManager;

        private AsyncTabsIncognitoTabHost(AsyncTabParamsManager asyncTabParamsManager) {
            mAsyncTabParamsManager = asyncTabParamsManager;
        }

        @Override
        public boolean hasIncognitoTabs() {
            SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
            for (int i = 0; i < asyncTabParams.size(); i++) {
                Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
                if (tab != null && tab.isIncognito()) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public void closeAllIncognitoTabs() {
            SparseArray<AsyncTabParams> asyncTabParams = mAsyncTabParamsManager.getAsyncTabParams();
            for (int i = 0; i < asyncTabParams.size(); i++) {
                Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
                if (tab != null && tab.isIncognito()) {
                    mAsyncTabParamsManager.remove(tab.getId());
                }
            }
        }
    }
}
