// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator implements StartSurface.Controller, TabSwitcher.OverviewModeObserver {
    /** Interface to control overlay visibility. */
    interface OverlayVisibilityHandler {
        /**
         * Set the content overlay visibility.
         * @param isVisible Whether the content overlay should be visible.
         */
        void setContentOverlayVisibility(boolean isVisible);
    }

    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    private final OverlayVisibilityHandler mOverlayVisibilityHandler;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    private final boolean mOnlyShowExploreSurface;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            OverlayVisibilityHandler overlayVisibilityHandler,
            @Nullable PropertyModel propertyModel,
            @Nullable ExploreSurfaceCoordinator.FeedSurfaceCreator feedSurfaceCreator,
            boolean onlyShowExploreSurface) {
        mController = controller;
        mOverlayVisibilityHandler = overlayVisibilityHandler;
        mPropertyModel = propertyModel;
        mFeedSurfaceCreator = feedSurfaceCreator;
        mOnlyShowExploreSurface = onlyShowExploreSurface;

        if (mPropertyModel != null) {
            mPropertyModel.set(
                    BOTTOM_BAR_CLICKLISTENER, new StartSurfaceProperties.BottomBarClickListener() {
                        // TODO(crbug.com/982018): Animate switching between explore and home
                        // surface.
                        @Override
                        public void onHomeButtonClicked() {
                            setExploreSurfaceVisibility(false);
                        }

                        @Override
                        public void onExploreButtonClicked() {
                            // TODO(crbug.com/982018): Hide the Tab switcher toolbar when showing
                            // explore surface.
                            setExploreSurfaceVisibility(true);
                        }
                    });

            tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    updateIncognitoMode(newModel.isIncognito());
                }
            });

            // Set the initial state.
            updateIncognitoMode(tabModelSelector.isIncognitoSelected());
        }

        mController.addOverviewModeObserver(this);
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mController.overviewVisible();
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
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        mController.showOverview(animate);

        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            if (mOnlyShowExploreSurface) mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);

            // Make sure FeedSurfaceCoordinator is built before the explore surface is showing by
            // default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
                mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                        mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                                mPropertyModel.get(IS_INCOGNITO)));
            }
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        }
    }

    @Override
    public boolean onBackPressed() {
        if (mPropertyModel != null && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && !mOnlyShowExploreSurface) {
            setExploreSurfaceVisibility(false);
            return true;
        }
        return mController.onBackPressed();
    }

    // Implements TabSwitcher.OverviewModeObserver.
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
        mOverlayVisibilityHandler.setContentOverlayVisibility(false);
    }

    @Override
    public void startedHiding() {
        mOverlayVisibilityHandler.setContentOverlayVisibility(true);
        if (mPropertyModel != null) {
            mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
            destroyFeedSurfaceCoordinator();
        }
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

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                    mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                            mPropertyModel.get(IS_INCOGNITO)));
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        // Update the 'BOTTOM_BAR_SELECTED_TAB_POSITION' property to reflect the change. This is
        // needed when clicking back button on the explore surface.
        mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, isVisible ? 1 : 0);
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mPropertyModel.get(IS_INCOGNITO)) return;

        mPropertyModel.set(IS_INCOGNITO, isIncognito);

        // Set invisible to remove the view from it's parent if it was showing and destroy the feed
        // surface coordinator (no matter it was shown or not).
        boolean wasShown = mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mPropertyModel.get(FEED_SURFACE_COORDINATOR) != null;
        if (wasShown) setExploreSurfaceVisibility(false);
        destroyFeedSurfaceCoordinator();

        // Set visible if it was shown, which will build the new feed surface coordinator according
        // to the new incognito state.
        if (wasShown) setExploreSurfaceVisibility(true);
    }

    private void destroyFeedSurfaceCoordinator() {
        FeedSurfaceCoordinator feedSurfaceCoordinator =
                mPropertyModel.get(FEED_SURFACE_COORDINATOR);
        if (feedSurfaceCoordinator != null) feedSurfaceCoordinator.destroy();
        mPropertyModel.set(FEED_SURFACE_COORDINATOR, null);
    }
}