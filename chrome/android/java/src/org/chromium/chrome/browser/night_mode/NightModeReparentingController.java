// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.util.SparseArray;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

// TODO(wylieb): Write unittests for this class.
/** Controls the reparenting of tabs when the theme is swapped. */
public class NightModeReparentingController
        implements StartStopWithNativeObserver, NightModeStateProvider.Observer {
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

    @Override
    public void onStartWithNative() {
        // Note: for now only the current tab is added to the AsyncTabParamsManager when the theme
        // is changed. In the future these will be added in tab index order and read at reverse
        // tab index order.
        final SparseArray<AsyncTabParams> paramsArray = AsyncTabParamsManager.getAsyncTabParams();
        for (int i = 0; i < paramsArray.size(); i++) {
            final int tabId = paramsArray.keyAt(i);
            AsyncTabParams params = paramsArray.get(tabId);
            if (!(params instanceof TabReparentingParams)) continue;
            if (params == null) continue;

            final TabReparentingParams reparentingParams = (TabReparentingParams) params;
            if (!reparentingParams.isFromNightModeReparenting()) continue;
            if (!reparentingParams.hasTabToReparent()) continue;
            if (!reparentingParams.hasTabIndex()) continue;

            final ReparentingTask reparentingTask =
                    ReparentingTask.get(reparentingParams.getTabToReparent());
            if (reparentingTask == null) continue;

            final TabModel tabModel = mDelegate.getTabModelSelector().getModel(
                    reparentingParams.getTabToReparent().isIncognito());
            if (tabModel == null) {
                AsyncTabParamsManager.remove(tabId);
                return;
            }

            reparentingTask.finish(mReparentingDelegate, () -> {
                tabModel.addTab(reparentingParams.getTabToReparent(),
                        reparentingParams.getTabIndex(), TabLaunchType.FROM_REPARENTING);
                AsyncTabParamsManager.remove(tabId);
            });
        }
    }

    @Override
    public void onStopWithNative() {}

    @Override
    public void onNightModeStateChanged() {
        // TODO(crbug.com/1031332): Reparent all tabs in the current tab model.
        Tab tabToDetach = mDelegate.getActivityTabProvider().get();
        if (tabToDetach == null) return;
        TabModel currentTabModel = mDelegate.getTabModelSelector().getCurrentModel();
        if (currentTabModel == null) return;

        TabReparentingParams params = new TabReparentingParams(tabToDetach, null, null);
        params.setTabIndex(currentTabModel.indexOf(tabToDetach));
        params.setFromNightModeReparenting(true);

        AsyncTabParamsManager.add(tabToDetach.getId(), params);
        currentTabModel.removeTab(tabToDetach);
        ReparentingTask.from(tabToDetach).detach();
    }
}
