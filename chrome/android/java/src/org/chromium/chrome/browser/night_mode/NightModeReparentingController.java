// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.util.SparseArray;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// TODO(wylieb): Write unittests for this class.
/** Controls the reparenting of tabs when the theme is swapped. */
public class NightModeReparentingController implements NightModeStateProvider.Observer {
    /** Provides data to {@link NightModeReparentingController} facilitate reparenting tabs. */
    public interface Delegate {
        /** The current ActivityTabProvider which is used to get the current Tab. */
        ActivityTabProvider getActivityTabProvider();

        /** Gets a {@link TabModelSelector} which is used to add the tab. */
        TabModelSelector getTabModelSelector();
    }

    private Delegate mDelegate;
    private ReparentingTask.Delegate mReparentingDelegate;

    /** Constructs a {@link NightModeReparentingController} with the given delegate. */
    public NightModeReparentingController(
            @NonNull Delegate delegate, @NonNull ReparentingTask.Delegate reparentingDelegate) {
        mDelegate = delegate;
        mReparentingDelegate = reparentingDelegate;
    }

    public void onNativeInitialized() {
        // Iterate through the params stored in AsyncTabParams and find the tabs stored by
        // #onNightModeStateChanged. Reparent the background tabs and store the foreground tab
        // to be reparented last.
        TabReparentingParams foregroundTabParams = null;
        SparseArray<AsyncTabParams> paramsArray = AsyncTabParamsManager.getAsyncTabParams().clone();
        for (int i = 0; i < paramsArray.size(); i++) {
            int tabId = paramsArray.keyAt(i);
            AsyncTabParams params = paramsArray.get(tabId);
            if (!(params instanceof TabReparentingParams)) continue;

            final TabReparentingParams reparentingParams = (TabReparentingParams) params;
            if (!reparentingParams.isFromNightModeReparenting()) continue;
            if (!reparentingParams.hasTabToReparent()) continue;

            final ReparentingTask reparentingTask =
                    ReparentingTask.get(reparentingParams.getTabToReparent());
            if (reparentingTask == null) continue;

            final TabModel tabModel = mDelegate.getTabModelSelector().getModel(
                    reparentingParams.getTabToReparent().isIncognito());
            if (tabModel == null) {
                AsyncTabParamsManager.remove(tabId);
                return;
            }

            if (reparentingParams.isForegroundTab()) {
                foregroundTabParams = reparentingParams;
            } else {
                reattachTab(reparentingTask, reparentingParams, tabModel);
            }
        }

        if (foregroundTabParams == null) return;
        final TabModel tabModel = mDelegate.getTabModelSelector().getModel(
                foregroundTabParams.getTabToReparent().isIncognito());
        if (tabModel == null) return;

        ReparentingTask task = ReparentingTask.get(foregroundTabParams.getTabToReparent());
        reattachTab(task, foregroundTabParams, tabModel);
    }

    /** Reattach the given task/pair to the activity/tab-model. */
    private void reattachTab(ReparentingTask task, TabReparentingParams params, TabModel tabModel) {
        final Tab tab = params.getTabToReparent();
        task.finish(mReparentingDelegate, () -> {
            tabModel.addTab(tab, params.getTabIndex(), TabLaunchType.FROM_REPARENTING);
            AsyncTabParamsManager.remove(tab.getId());
        });
    }

    @Override
    public void onNightModeStateChanged() {
        Tab foregroundTab = mDelegate.getActivityTabProvider().get();
        Map<TabModel, List<Tab>> tabsAndModels = getTabModelAndTabs();

        for (final TabModel model : tabsAndModels.keySet()) {
            List<Tab> tabs = tabsAndModels.get(model);
            for (int i = 0; i < tabs.size(); i++) {
                Tab tab = tabs.get(i);
                TabReparentingParams params = new TabReparentingParams(tab, null, null);
                params.setFromNightModeReparenting(true);

                // Only the first tab is considered foreground and that is the only one for which
                // we need an index. It will be reattached at the end. All remaining tabs will be
                // reattached in reverse order, therefore we can ignore the index.
                if (tab == foregroundTab) {
                    params.setIsForegroundTab(true);
                    params.setTabIndex(i);
                } else {
                    params.setIsForegroundTab(false);
                }

                AsyncTabParamsManager.add(tab.getId(), params);
                ReparentingTask.from(tab).detach();
            }
        }

        // Save the current state to overwrite tab state that was previously saved to disk. See
        // crbug.com/1042565 for more info.
        mDelegate.getTabModelSelector().saveState();
    }

    /** @return Mapping from TabModel to all the Tabs in that model. */
    Map<TabModel, List<Tab>> getTabModelAndTabs() {
        Map<TabModel, List<Tab>> tabsAndModels = new HashMap<>();

        TabModel model = mDelegate.getTabModelSelector().getModel(false);
        tabsAndModels.put(model, getTabsForModel(model));
        model = mDelegate.getTabModelSelector().getModel(true);
        tabsAndModels.put(model, getTabsForModel(model));

        return tabsAndModels;
    }

    /** @return All of a model's Tabs as a List. */
    List<Tab> getTabsForModel(TabModel model) {
        List<Tab> tabs = new ArrayList<>();
        if (model == null) return tabs;

        TabList tabList = model.getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            tabs.add(tabList.getTabAt(i));
        }

        return tabs;
    }
}
