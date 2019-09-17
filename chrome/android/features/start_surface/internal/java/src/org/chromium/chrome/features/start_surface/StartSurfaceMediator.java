// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements StartSurface.Controller, TabSwitcher.OverviewModeObserver, View.OnClickListener {
    /** Interface to initialize a secondary tasks surface for more tabs. */
    interface SecondaryTasksSurfaceInitializer {
        /**
         * Initialize the secondary tasks surface and return the surface controller, which is
         * TabSwitcher.Controller.
         * @return The {@link TabSwitcher.Controller} of the secondary tasks surface.
         */
        TabSwitcher.Controller initialize();
    }

    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    @Nullable
    private final SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    private final boolean mOnlyShowExploreSurface;
    @Nullable
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    private boolean mIsIncognito;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable ExploreSurfaceCoordinator.FeedSurfaceCreator feedSurfaceCreator,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            @StartSurfaceCoordinator.SurfaceMode int mode) {
        mController = controller;
        mPropertyModel = propertyModel;
        mFeedSurfaceCreator = feedSurfaceCreator;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mOnlyShowExploreSurface = mode == StartSurfaceCoordinator.SurfaceMode.SINGLE_PANE;

        if (mPropertyModel != null) {
            mPropertyModel.set(
                    BOTTOM_BAR_CLICKLISTENER, new StartSurfaceProperties.BottomBarClickListener() {
                        // TODO(crbug.com/982018): Animate switching between explore and home
                        // surface.
                        @Override
                        public void onHomeButtonClicked() {
                            setExploreSurfaceVisibility(false);
                            RecordUserAction.record("StartSurface.TwoPanes.BottomBar.TapHome");
                        }

                        @Override
                        public void onExploreButtonClicked() {
                            // TODO(crbug.com/982018): Hide the Tab switcher toolbar when showing
                            // explore surface.
                            setExploreSurfaceVisibility(true);
                            RecordUserAction.record(
                                    "StartSurface.TwoPanes.BottomBar.TapExploreSurface");
                        }
                    });

            mIsIncognito = tabModelSelector.isIncognitoSelected();
            tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    updateIncognitoMode(newModel.isIncognito());
                }
            });

            // Set the initial state.

            // Show explore surface if not in incognito and either in SINGLE PANES mode
            // or in TWO PANES mode with last visible pane explore.
            boolean shouldShowExploreSurface =
                    (mOnlyShowExploreSurface || ReturnToStartSurfaceUtil.shouldShowExploreSurface())
                    && !mIsIncognito;
            setExploreSurfaceVisibility(shouldShowExploreSurface);
            if (mode != StartSurfaceCoordinator.SurfaceMode.SINGLE_PANE
                    && mode != StartSurfaceCoordinator.SurfaceMode.TASKS_ONLY) {
                mPropertyModel.set(BOTTOM_BAR_HEIGHT,
                        ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                                R.dimen.ss_bottom_bar_height));
                mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, !mIsIncognito);

                // Margin the bottom of the Tab grid to save space for the bottom bar when visible.
                mController.setBottomControlsHeight(
                        mIsIncognito ? 0 : mPropertyModel.get(BOTTOM_BAR_HEIGHT));
            }

            if (mode == StartSurfaceCoordinator.SurfaceMode.TWO_PANES) {
                int toolbarHeight =
                        ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                                R.dimen.toolbar_height_no_shadow);
                mPropertyModel.set(TOP_BAR_HEIGHT, toolbarHeight * 2);
            }
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
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()) {
            assert mOnlyShowExploreSurface;

            setSecondaryTasksSurfaceVisibility(false);
        }
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            // There are only two modes (single pane, when mOnlyShowExploreSurface == true, and two
            // panes) available when mPropertyModel != null.
            if (mOnlyShowExploreSurface) {
                RecordUserAction.record("StartSurface.SinglePane");
                if (mIsIncognito) {
                    setSecondaryTasksSurfaceVisibility(true);
                } else {
                    setExploreSurfaceVisibility(true);
                }
            } else {
                RecordUserAction.record("StartSurface.TwoPanes");
                String defaultOnUserActionString = mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                        ? "ExploreSurface"
                        : "HomeSurface";
                RecordUserAction.record(
                        "StartSurface.TwoPanes.DefaultOn" + defaultOnUserActionString);
            }

            // Make sure FeedSurfaceCoordinator is built before the explore surface is showing by
            // default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
                mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                        mFeedSurfaceCreator.createFeedSurfaceCoordinator());
            }
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        }

        mController.showOverview(animate);
    }

    @Override
    public boolean onBackPressed() {
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()
                // Secondary tasks surface is used as the main surface in incognito mode.
                && !mIsIncognito) {
            assert mOnlyShowExploreSurface;

            setSecondaryTasksSurfaceVisibility(false);
            setExploreSurfaceVisibility(true);
            return true;
        }

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
    }

    @Override
    public void startedHiding() {
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

    // Implements View.OnClickListener, which listens for the more tabs button.
    @Override
    public void onClick(View v) {
        assert mOnlyShowExploreSurface;

        if (mSecondaryTasksSurfaceController == null) {
            mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
        }

        setExploreSurfaceVisibility(false);
        setSecondaryTasksSurfaceVisibility(true);
        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
            mPropertyModel.set(
                    FEED_SURFACE_COORDINATOR, mFeedSurfaceCreator.createFeedSurfaceCoordinator());
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        if (!mOnlyShowExploreSurface) {
            // Update the 'BOTTOM_BAR_SELECTED_TAB_POSITION' property to reflect the change. This is
            // needed when clicking back button on the explore surface.
            mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, isVisible ? 1 : 0);
            ReturnToStartSurfaceUtil.setExploreSurfaceVisibleLast(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        if (mOnlyShowExploreSurface) {
            setExploreSurfaceVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(
                    mIsIncognito && mPropertyModel.get(IS_SHOWING_OVERVIEW));
        } else {
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, !mIsIncognito);

            // Set bottom controls height to 0 when bottom bar is hidden in incogito mode
            mController.setBottomControlsHeight(
                    mIsIncognito ? 0 : mPropertyModel.get(BOTTOM_BAR_HEIGHT));

            // Hide explore surface if going incognito. When returning to normal mode, we
            // always show the Home Pane, so the Explore Pane stays hidden.
            if (mIsIncognito) setExploreSurfaceVisibility(false);
        }
    }

    private void destroyFeedSurfaceCoordinator() {
        FeedSurfaceCoordinator feedSurfaceCoordinator =
                mPropertyModel.get(FEED_SURFACE_COORDINATOR);
        if (feedSurfaceCoordinator != null) feedSurfaceCoordinator.destroy();
        mPropertyModel.set(FEED_SURFACE_COORDINATOR, null);
    }

    private void setSecondaryTasksSurfaceVisibility(boolean isVisible) {
        assert mOnlyShowExploreSurface;
        if (isVisible) {
            if (mSecondaryTasksSurfaceController == null) {
                mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            }
            mSecondaryTasksSurfaceController.showOverview(false);
        } else {
            if (mSecondaryTasksSurfaceController == null) return;
            mSecondaryTasksSurfaceController.hideOverview(false);
        }
    }
}
