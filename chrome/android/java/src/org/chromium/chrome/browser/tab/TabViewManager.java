// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.util.SparseIntArray;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;

import java.util.Comparator;
import java.util.PriorityQueue;

/**
 * TODO(crbug.com/1074348) Make this an interface and move it to //chrome/browser/tab.
 * This class is responsible for displaying custom {@link View}s on top of {@link Tab}'s Content
 * view. Users that want to display a custom {@link View} should:
 *     1. Implement {@link TabViewProvider}
 *     2. Add an entry to {@link TabViewProvider.Type}
 *     3. Add their {@link TabViewProvider.Type} to {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}
 *        with an appropriate priority. In order to find the right priority, please consider the
 *        existing entries in the array and determine where the new feature fits relative to them.
 *     4. Use {@link #addTabViewProvider} and {@link #removeTabViewProvider} to add and remove their
 *        {@link TabViewProvider}
 */
public class TabViewManager implements UserData, Comparator<TabViewProvider> {
    /**
     * A prioritized list of all {@link TabViewProvider.Type}s, from most important to least
     * important. The {@link TabViewProvider} with the highest priority will always be shown first,
     * regardless of its insertion time relative to other {@link TabViewProvider}s.
     */
    @VisibleForTesting
    @TabViewProvider.Type
    static final int[] PRIORITIZED_TAB_VIEW_PROVIDER_TYPES = new int[] {
        TabViewProvider.Type.SUSPENDED_TAB,
        TabViewProvider.Type.PAINT_PREVIEW,
        TabViewProvider.Type.SAD_TAB};

    /**
     * A lookup table for {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}. This is initialized in the
     * following static block and doesn't need to be manually updated.
     */
    private static final SparseIntArray TAB_VIEW_PROVIDER_PRIORITY_LOOKUP = new SparseIntArray();
    static {
        for (int i = 0; i < PRIORITIZED_TAB_VIEW_PROVIDER_TYPES.length; i++) {
            TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.put(PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[i], i);
        }
    }
    private static final Class<TabViewManager> USER_DATA_KEY = TabViewManager.class;

    private PriorityQueue<TabViewProvider> mTabViewProviders;
    private TabImpl mTab;

    public static TabViewManager get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabViewManager(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    @VisibleForTesting
    TabViewManager(Tab tab) {
        mTab = (TabImpl) tab;
        mTabViewProviders = new PriorityQueue<>(PRIORITIZED_TAB_VIEW_PROVIDER_TYPES.length, this);
    }

    /**
     * @return The {@link TabViewProvider} that is being currently displayed.
     */
    public @Nullable TabViewProvider getCurrentTabViewProvider() {
        return mTabViewProviders.peek();
    }

    /**
     * Adds a {@link TabViewProvider} to be shown in the {@link Tab} associated with this {@link
     * TabViewManager}. If the given {@link TabViewProvider} has the highest priority, it will be
     * shown immediately. Otherwise, it will be shown after other {@link TabViewProvider}s with
     * higher priorities are removed.
     */
    public void addTabViewProvider(TabViewProvider tabViewProvider) {
        if (mTabViewProviders.contains(tabViewProvider)) return;

        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        mTabViewProviders.add(tabViewProvider);
        updateCurrentTabViewProvider(currentTabViewProvider);
    }

    /**
     * Remove the given {@link TabViewProvider} from the {@link Tab} associated with this {@link
     * TabViewManager}. If the given {@link TabViewProvider} is currently shown, the next available
     * {@link TabViewProvider} with the highest priority will be shown. If there are no other {@link
     * TabViewProvider}s, {@link Tab}'s Content view will be shown.
     */
    public void removeTabViewProvider(TabViewProvider tabViewProvider) {
        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        mTabViewProviders.remove(tabViewProvider);
        updateCurrentTabViewProvider(currentTabViewProvider);
    }

    private void updateCurrentTabViewProvider(TabViewProvider previousTabViewProvider) {
        if (mTab == null) return;

        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        if (currentTabViewProvider != previousTabViewProvider) {
            View view = null;
            if (currentTabViewProvider != null) {
                view = currentTabViewProvider.getView();
                assert view != null;
            }
            mTab.setCustomView(view);
            if (previousTabViewProvider != null) previousTabViewProvider.onHidden();
            if (currentTabViewProvider != null) currentTabViewProvider.onShown();
        }
    }

    /**
     * Compares two {@link TabViewProvider}s based on their priority in
     * {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}. Do not edit the logic here when you add a new
     * {@link TabViewProvider.Type}. Instead, simply add your new {@link TabViewProvider.Type} to
     * {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}.
     */
    @Override
    public int compare(TabViewProvider tvp1, TabViewProvider tvp2) {
        int tvp1Priority = TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.get(tvp1.getTabViewProviderType());
        int tvp2Priority = TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.get(tvp2.getTabViewProviderType());
        return tvp1Priority - tvp2Priority;
    }

    @Override
    public void destroy() {
        mTab.setCustomView(null);
        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        if (currentTabViewProvider != null) currentTabViewProvider.onHidden();
        mTabViewProviders.clear();
        mTab = null;
    }
}
