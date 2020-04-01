// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.ArrayList;
import java.util.List;

/** Controls the reparenting of tabs when the theme is swapped. */
public class NightModeReparentingController implements NightModeStateProvider.Observer {
    /** Provides data to {@link NightModeReparentingController} facilitate reparenting tabs. */
    public interface Delegate {
        /** The current ActivityTabProvider which is used to get the current Tab. */
        ActivityTabProvider getActivityTabProvider();

        /** Gets a {@link TabModelSelector} which is used to add the tab. */
        TabModelSelector getTabModelSelector();

        /** @return Whether the given Url is an NTP url, exists solely to support unit testing. */
        boolean isNTPUrl(String url);
    }

    private Delegate mDelegate;

    /** Constructs a {@link NightModeReparentingController} with the given delegate. */
    public NightModeReparentingController(@NonNull Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onNightModeStateChanged() {
        // TODO(crbug.com/1065201): Make tab models detachable.
        TabModelSelector selector = mDelegate.getTabModelSelector();

        // Close tabs pending closure before saving params.
        selector.getModel(false).commitAllTabClosures();
        selector.getModel(true).commitAllTabClosures();

        // Aggregate all the tabs.
        List<Tab> tabs = new ArrayList<>(selector.getTotalTabCount());
        populateComprehensiveTabsFromModel(selector.getModel(false), tabs);
        populateComprehensiveTabsFromModel(selector.getModel(true), tabs);

        // Save all the tabs in memory to be retrieved after restart.
        mDelegate.getTabModelSelector().enterReparentingMode();
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);

            // Intentionally skip new tab pages and allow them to reload and restore scroll
            // state themselves.
            if (mDelegate.isNTPUrl(tab.getUrlString())) continue;

            TabReparentingParams params = new TabReparentingParams(tab, null, null);
            AsyncTabParamsManager.add(tab.getId(), params);
            ReparentingTask.from(tab).detach();
        }
    }

    static void populateComprehensiveTabsFromModel(TabModel model, List<Tab> outputTabs) {
        TabList tabList = model.getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            outputTabs.add(tabList.getTabAt(i));
        }
    }
}
