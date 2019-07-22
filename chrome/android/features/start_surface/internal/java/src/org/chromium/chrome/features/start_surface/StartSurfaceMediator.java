// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements StartSurface.Controller, GridTabSwitcher.GridOverviewModeObserver {
    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final GridTabSwitcher.GridController mGridController;
    @Nullable
    private final BottomBarCoordinator mBottomBarCoordinator;
    @Nullable
    private final ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    StartSurfaceMediator(GridTabSwitcher.GridController gridController,
            @Nullable BottomBarCoordinator bottomBarCoordinator,
            @Nullable ExploreSurfaceCoordinator exploreSurfaceCoordinator) {
        mGridController = gridController;
        mBottomBarCoordinator = bottomBarCoordinator;
        mExploreSurfaceCoordinator = exploreSurfaceCoordinator;

        if (mBottomBarCoordinator != null) {
            mBottomBarCoordinator.setOnClickListener(new BottomBarProperties.OnClickListener() {
                @Override
                public void onHomeButtonClicked() {
                    // TODO(crbug.com/982018): Show home surface and hide explore surface.
                }

                @Override
                public void onExploreButtonClicked() {
                    // TODO(crbug.com/982018): Show explore surface and hide home surface.
                }
            });
        }
        mGridController.addOverviewModeObserver(this);
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mGridController.overviewVisible();
    }

    @Override
    public void addOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        mGridController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        mGridController.showOverview(animate);

        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mBottomBarCoordinator != null) mBottomBarCoordinator.setVisibility(true);
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug.com/982018): Check whether explore surface is shown, if yes, switch back to
        // home surface.
        return mGridController.onBackPressed();
    }

    // Implements GridTabSwitcher.GridOverviewModeObserver.
    @Override
    public void startedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding() {
        if (mBottomBarCoordinator != null) mBottomBarCoordinator.setVisibility(false);
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }
}