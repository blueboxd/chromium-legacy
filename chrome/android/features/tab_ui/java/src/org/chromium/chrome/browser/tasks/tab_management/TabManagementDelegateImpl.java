// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.metrics.UmaSessionStats;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModule")
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public Layout createGTSLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, GridTabSwitcher gridTabSwitcher) {
        return new GridTabSwitcherLayout(context, updateHost, renderHost, gridTabSwitcher);
    }

    @Override
    public GridTabSwitcher createGridTabSwitcher(ChromeActivity activity) {
        if (ChromeFeatureList.isInitialized()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        return new GridTabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getTabModelSelector(), activity.getTabContentManager(),
                activity.getCompositorViewHolder(), activity.getFullscreenManager(), activity,
                activity.getMenuOrKeyboardActionController(), activity::onBackPressed);
    }

    @Override
    public TabGroupUi createTabGroupUi(
            ViewGroup parentView, ThemeColorProvider themeColorProvider) {
        return new TabGroupUiCoordinator(parentView, themeColorProvider);
    }
}
