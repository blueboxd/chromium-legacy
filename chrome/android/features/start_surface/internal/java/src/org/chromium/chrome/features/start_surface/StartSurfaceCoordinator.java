// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.ViewGroup;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final GridTabSwitcher mGridTabSwitcher;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private BottomBarCoordinator mBottomBarCoordinator;
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;
    private PropertyModel mPropertyModel;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mGridTabSwitcher =
                TabManagementModuleProvider.getDelegate().createGridTabSwitcher(activity);

        // Do not enable this feature when the bottom bar is enabled since it will
        // overlap the start surface's bottom bar.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TWO_PANES_START_SURFACE_ANDROID)
                && !FeatureUtilities.isBottomToolbarEnabled()) {
            // Margin the bottom of the Tab grid to save space for the bottom bar.
            int bottomBarHeight =
                    ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                            R.dimen.ss_bottom_bar_height);
            mGridTabSwitcher.getTabGridDelegate().setBottomControlsHeight(bottomBarHeight);

            mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
            mPropertyModel.set(BOTTOM_BAR_HEIGHT, bottomBarHeight);
            int singleToolbarHeight =
                    activity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
            int toolbarHeight = ReturnToChromeExperimentsUtil.shouldShowOmniboxOnTabSwitcher()
                    ? singleToolbarHeight
                    : 0;
            mPropertyModel.set(TOP_BAR_HEIGHT, toolbarHeight);

            // Create the bottom bar.
            mBottomBarCoordinator = new BottomBarCoordinator(
                    activity, activity.getCompositorViewHolder(), mPropertyModel);

            // Create the explore surface.
            // TODO(crbug.com/982018): This is a hack to hide the top tab switcher toolbar in
            // the explore surface. Remove it after deciding on where to put the omnibox.
            ViewGroup exploreSurfaceContainer =
                    (ViewGroup) activity.getCompositorViewHolder().getParent();
            mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator(
                    activity, exploreSurfaceContainer, mPropertyModel);
        }

        mStartSurfaceMediator = new StartSurfaceMediator(mGridTabSwitcher.getGridController(),
                activity.getTabModelSelector(), mPropertyModel,
                mExploreSurfaceCoordinator == null
                        ? null
                        : mExploreSurfaceCoordinator.getFeedSurfaceCreator());
    }

    // Implements StartSurface.
    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        mGridTabSwitcher.setOnTabSelectingListener(
                (GridTabSwitcher.OnTabSelectingListener) listener);
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public GridTabSwitcher.TabGridDelegate getTabGridDelegate() {
        return mGridTabSwitcher.getTabGridDelegate();
    }
}