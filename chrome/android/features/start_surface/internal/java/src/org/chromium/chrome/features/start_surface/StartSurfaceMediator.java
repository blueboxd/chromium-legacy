// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements StartSurface.Controller, TabSwitcher.OverviewModeObserver, View.OnClickListener {
    @IntDef({SurfaceMode.NO_START_SURFACE, SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES,
            SurfaceMode.SINGLE_PANE, SurfaceMode.OMNIBOX_ONLY})
    @Retention(RetentionPolicy.SOURCE)
    @interface SurfaceMode {
        int NO_START_SURFACE = 0;
        int TASKS_ONLY = 1;
        int TWO_PANES = 2;
        int SINGLE_PANE = 3;
        int OMNIBOX_ONLY = 4;
    }

    /** Interface to initialize a secondary tasks surface for more tabs. */
    interface SecondaryTasksSurfaceInitializer {
        /**
         * Initialize the secondary tasks surface and return the surface controller, which is
         * TabSwitcher.Controller.
         * @return The {@link TabSwitcher.Controller} of the secondary tasks surface.
         */
        TabSwitcher.Controller initialize();
    }

    /**
     * Interface to check the associated activity state.
     */
    interface ActivityStateChecker {
        /**
         * @return Whether the associated activity is finishing or destroyed.
         */
        boolean isFinishingOrDestroyed();
    }

    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    private final TabModelSelector mTabModelSelector;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    @Nullable
    private final SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    @SurfaceMode
    private final int mSurfaceMode;
    @Nullable
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Nullable
    private PropertyModel mSecondaryTasksSurfacePropertyModel;
    private boolean mIsIncognito;
    @Nullable
    private FakeboxDelegate mFakeboxDelegate;
    private NightModeStateProvider mNightModeStateProvider;
    @Nullable
    UrlFocusChangeListener mUrlFocusChangeListener;
    @Nullable
    private StartSurface.StateObserver mStateObserver;
    @OverviewModeState
    private int mOverviewModeState;
    @OverviewModeState
    private int mPreviousOverviewModeState;
    private boolean mIsOmniboxFocused;
    @Nullable
    private TabModel mNormalTabModel;
    @Nullable
    private TabModelObserver mNormalTabModelObserver;
    @Nullable
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private ChromeFullscreenManager mFullScreenManager;
    private ChromeFullscreenManager.FullscreenListener mFullScreenListener;
    private ActivityStateChecker mActivityStateChecker;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable ExploreSurfaceCoordinator.FeedSurfaceCreator feedSurfaceCreator,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            @SurfaceMode int surfaceMode, @Nullable FakeboxDelegate fakeboxDelegate,
            NightModeStateProvider nightModeStateProvider,
            ChromeFullscreenManager fullscreenManager, ActivityStateChecker activityStateChecker) {
        mController = controller;
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mFeedSurfaceCreator = feedSurfaceCreator;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mSurfaceMode = surfaceMode;
        mFakeboxDelegate = fakeboxDelegate;
        mNightModeStateProvider = nightModeStateProvider;
        mFullScreenManager = fullscreenManager;
        mActivityStateChecker = activityStateChecker;

        if (mPropertyModel != null) {
            assert mSurfaceMode == SurfaceMode.SINGLE_PANE || mSurfaceMode == SurfaceMode.TWO_PANES
                    || mSurfaceMode == SurfaceMode.TASKS_ONLY
                    || mSurfaceMode == SurfaceMode.OMNIBOX_ONLY;
            assert mFakeboxDelegate != null;

            mIsIncognito = mTabModelSelector.isIncognitoSelected();

            mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    // TODO(crbug.com/982018): Optimize to not listen for selected Tab model change
                    // when overview is not shown.
                    updateIncognitoMode(newModel.isIncognito());
                }
            };
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            if (mSurfaceMode == SurfaceMode.TWO_PANES) {
                mPropertyModel.set(BOTTOM_BAR_CLICKLISTENER,
                        new StartSurfaceProperties.BottomBarClickListener() {
                            // TODO(crbug.com/982018): Animate switching between explore and home
                            // surface.
                            @Override
                            public void onHomeButtonClicked() {
                                setExploreSurfaceVisibility(false);
                                notifyStateChange();
                                RecordUserAction.record("StartSurface.TwoPanes.BottomBar.TapHome");
                            }

                            @Override
                            public void onExploreButtonClicked() {
                                // TODO(crbug.com/982018): Hide the Tab switcher toolbar when
                                // showing explore surface.
                                setExploreSurfaceVisibility(true);
                                notifyStateChange();
                                RecordUserAction.record(
                                        "StartSurface.TwoPanes.BottomBar.TapExploreSurface");
                            }
                        });
            }

            if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
                mPropertyModel.set(MORE_TABS_CLICK_LISTENER, this);

                // Hide tab carousel, which does not exist in incognito mode, when closing all
                // normal tabs.
                mNormalTabModel = mTabModelSelector.getModel(false);
                mNormalTabModelObserver = new EmptyTabModelObserver() {
                    @Override
                    public void willCloseTab(Tab tab, boolean animate) {
                        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE
                                && mNormalTabModel.getCount() <= 1) {
                            setTabCarouselVisibility(false);
                        }
                    }
                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE) {
                            setTabCarouselVisibility(true);
                        }
                    }
                };

                mFullScreenListener = new ChromeFullscreenManager.FullscreenListener() {
                    @Override
                    public void onBottomControlsHeightChanged(
                            int bottomControlsHeight, int bottomControlsMinHeight) {
                        // Only pad single pane home page since tabs grid has already been
                        // padded for the bottom bar.
                        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE) {
                            mPropertyModel.set(BOTTOM_BAR_HEIGHT, bottomControlsHeight);
                        }
                    }
                };
            }

            // Initialize
            // Note that isVoiceSearchEnabled will return false in incognito mode.
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mFakeboxDelegate.getLocationBarVoiceRecognitionHandler()
                            .isVoiceSearchEnabled());

            int toolbarHeight =
                    ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                            R.dimen.toolbar_height_no_shadow);
            mPropertyModel.set(TOP_BAR_HEIGHT, toolbarHeight);

            mUrlFocusChangeListener = new UrlFocusChangeListener() {
                @Override
                public void onUrlFocusChange(boolean hasFocus) {
                    if (hasFakeSearchBox()) {
                        if (mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
                            mSecondaryTasksSurfacePropertyModel.set(
                                    IS_FAKE_SEARCH_BOX_VISIBLE, !hasFocus);
                        } else {
                            setFakeBoxVisibility(!hasFocus);
                        }
                    }
                    notifyStateChange();
                }
            };
        }
        mController.addOverviewModeObserver(this);
        mPreviousOverviewModeState = OverviewModeState.NOT_SHOWN;
        mOverviewModeState = OverviewModeState.NOT_SHOWN;
    }

    void setSecondaryTasksSurfacePropertyModel(PropertyModel propertyModel) {
        mSecondaryTasksSurfacePropertyModel = propertyModel;
        mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);

        // Secondary tasks surface is used for more Tabs or incognito mode single pane, where MV
        // tiles and voice recognition button should be invisible.
        mSecondaryTasksSurfacePropertyModel.set(MV_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
    }

    void setStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObserver = observer;
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mController.overviewVisible();
    }

    @Override
    public void setOverviewState(@OverviewModeState int state) {
        // TODO(crbug.com/1039691): Refactor into state and trigger to separate SHOWING and SHOWN
        // states.

        if (mPropertyModel == null || state == mOverviewModeState) return;

        // Cache previous state.
        if (mOverviewModeState != OverviewModeState.NOT_SHOWN) {
            mPreviousOverviewModeState = mOverviewModeState;
        }

        mOverviewModeState = state;
        setOverviewStateInternal();

        // Immediately transition from SHOWING to SHOWN state if overview is visible but state not
        // SHOWN. This is only necessary when the new state is a SHOWING state.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mOverviewModeState != OverviewModeState.NOT_SHOWN
                && !isShownState(mOverviewModeState)) {
            // Compute SHOWN state before updating previous state, because the previous state is
            // still needed to compute the shown state.
            @OverviewModeState
            int shownState = computeOverviewStateShown();

            // Cache previous state
            mPreviousOverviewModeState = mOverviewModeState;

            mOverviewModeState = shownState;
            setOverviewStateInternal();
        }
        notifyStateChange();

        // Metrics collection
        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE) {
            RecordUserAction.record("StartSurface.SinglePane.Home");
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER) {
            RecordUserAction.record("StartSurface.SinglePane.Tabswitcher");
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES) {
            RecordUserAction.record("StartSurface.TwoPanes");
            String defaultOnUserActionString = mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    ? "ExploreSurface"
                    : "HomeSurface";
            RecordUserAction.record("StartSurface.TwoPanes.DefaultOn" + defaultOnUserActionString);
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY) {
            RecordUserAction.record("StartSurface.TasksOnly");
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY) {
            RecordUserAction.record("StartSurface.OmniboxOnly");
        }
    }

    private void setOverviewStateInternal() {
        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE) {
            setExploreSurfaceVisibility(!mIsIncognito);
            setTabCarouselVisibility(
                    mTabModelSelector.getModel(false).getCount() > 0 && !mIsIncognito);
            setMVTilesVisibility(!mIsIncognito);
            setFakeBoxVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(mIsIncognito);

            // Only pad single pane home page since tabs grid has already been padding for the
            // bottom bar.
            mPropertyModel.set(BOTTOM_BAR_HEIGHT, mFullScreenManager.getBottomControlsHeight());
            mNormalTabModel.addObserver(mNormalTabModelObserver);

        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER) {
            setExploreSurfaceVisibility(false);
            setTabCarouselVisibility(false);
            setMVTilesVisibility(false);
            setFakeBoxVisibility(false);
            setSecondaryTasksSurfaceVisibility(true);

        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES) {
            // Show Explore Surface if last visible pane explore.
            setExploreSurfaceVisibility(
                    ReturnToStartSurfaceUtil.shouldShowExploreSurface() && !mIsIncognito);
            setMVTilesVisibility(!mIsIncognito);
            mPropertyModel.set(BOTTOM_BAR_HEIGHT,
                    mIsIncognito ? 0
                                 : ContextUtils.getApplicationContext()
                                           .getResources()
                                           .getDimensionPixelSize(R.dimen.ss_bottom_bar_height));
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, !mIsIncognito);

        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY) {
            setMVTilesVisibility(!mIsIncognito);
            setExploreSurfaceVisibility(false);
            setFakeBoxVisibility(true);
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY) {
            setMVTilesVisibility(false);
            setExploreSurfaceVisibility(false);
            setFakeBoxVisibility(true);
        } else if (mOverviewModeState == OverviewModeState.NOT_SHOWN) {
            if (mSecondaryTasksSurfaceController != null) setSecondaryTasksSurfaceVisibility(false);
        }

        if (isShownState(mOverviewModeState)) {
            setIncognitoModeDescriptionVisibility(
                    mIsIncognito && mTabModelSelector.getModel(true).getCount() <= 0);
        }
    }

    @VisibleForTesting
    @OverviewModeState
    public int getOverviewState() {
        return mOverviewModeState;
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
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            RecordUserAction.record("StartSurface.Shown");

            // update incognito
            mIsIncognito = mTabModelSelector.isIncognitoSelected();
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            // if OvervieModeState is NOT_SHOWN, default to SHOWING_HOMEPAGE. This should only
            // happen when entering Start through SwipeDown gesture on URL bar.
            if (mOverviewModeState == OverviewModeState.NOT_SHOWN) {
                mOverviewModeState = OverviewModeState.SHOWING_HOMEPAGE;
            }

            // set OverviewModeState
            @OverviewModeState
            int shownState = computeOverviewStateShown();
            assert (isShownState(shownState));
            setOverviewState(shownState);

            // Make sure FeedSurfaceCoordinator is built before the explore surface is showing by
            // default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null
                    && !mActivityStateChecker.isFinishingOrDestroyed()) {
                mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                        mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                                mNightModeStateProvider.isInNightMode()));
            }
            mTabModelSelector.addObserver(mTabModelSelectorObserver);

            if (mFullScreenListener != null) {
                mFullScreenManager.addListener(mFullScreenListener);
            }

            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mFakeboxDelegate.addUrlFocusChangeListener(mUrlFocusChangeListener);
        }

        mController.showOverview(animate);
    }

    @Override
    public boolean onBackPressed() {
        if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER
                // Secondary tasks surface is used as the main surface in incognito mode.
                && !mIsIncognito) {
            // If we reached tabswitcher from HomePage.
            if (mPreviousOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE) {
                setOverviewState(OverviewModeState.SHOWN_HOMEPAGE);
                return true;
            }
        }

        if (mPropertyModel != null && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES) {
            setExploreSurfaceVisibility(false);
            notifyStateChange();
            return true;
        }

        return mController.onBackPressed();
    }

    @Override
    public void enableRecordingFirstMeaningfulPaint(long activityCreateTimeMs) {
        mController.enableRecordingFirstMeaningfulPaint(activityCreateTimeMs);
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
            mFakeboxDelegate.removeUrlFocusChangeListener(mUrlFocusChangeListener);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, false);

            destroyFeedSurfaceCoordinator();
            if (mNormalTabModelObserver != null) {
                mNormalTabModel.removeObserver(mNormalTabModelObserver);
            }
            if (mTabModelSelectorObserver != null) {
                mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            }
            if (mFullScreenListener != null) {
                mFullScreenManager.removeListener(mFullScreenListener);
            }
            setOverviewState(OverviewModeState.NOT_SHOWN);
            RecordUserAction.record("StartSurface.Hidden");
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

    private void destroyFeedSurfaceCoordinator() {
        FeedSurfaceCoordinator feedSurfaceCoordinator =
                mPropertyModel.get(FEED_SURFACE_COORDINATOR);
        if (feedSurfaceCoordinator != null) feedSurfaceCoordinator.destroy();
        mPropertyModel.set(FEED_SURFACE_COORDINATOR, null);
    }

    // TODO(crbug.com/982018): turn into onClickMoreTabs() and hide the OnClickListener signature
    // inside. Implements View.OnClickListener, which listens for the more tabs button.
    @Override
    public void onClick(View v) {
        assert mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE;

        if (mSecondaryTasksSurfaceController == null) {
            mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            assert mSecondaryTasksSurfacePropertyModel != null;
        }

        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
        setOverviewState(OverviewModeState.SHOWN_TABSWITCHER);
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null
                && !mActivityStateChecker.isFinishingOrDestroyed()) {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                    mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                            mNightModeStateProvider.isInNightMode()));
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES) {
            // Update the 'BOTTOM_BAR_SELECTED_TAB_POSITION' property to reflect the change. This is
            // needed when clicking back button on the explore surface.
            mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, isVisible ? 1 : 0);
            ReturnToStartSurfaceUtil.setExploreSurfaceVisibleLast(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        setOverviewStateInternal();

        // TODO(crbug.com/1021399): This looks not needed since there is no way to change incognito
        // mode when focusing on the omnibox and incognito mode change won't affect the visibility
        // of the tab switcher toolbar.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) notifyStateChange();
    }

    private void setSecondaryTasksSurfaceVisibility(boolean isVisible) {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;

        if (isVisible) {
            if (mSecondaryTasksSurfaceController == null) {
                mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            }
            mSecondaryTasksSurfacePropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE,
                    mIsIncognito && mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE);
            mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);
            mSecondaryTasksSurfaceController.showOverview(false);
        } else {
            if (mSecondaryTasksSurfaceController == null) return;
            mSecondaryTasksSurfaceController.hideOverview(false);
        }
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, isVisible);
    }

    private void notifyStateChange() {
        if (mStateObserver != null) {
            mStateObserver.onStateChanged(mOverviewModeState, shouldShowTabSwitcherToolbar());
        }
    }

    private boolean hasFakeSearchBox() {
        // No fake search box on the explore pane in two panes mode.
        if (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE
                || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY
                || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                || (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES
                        && !mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE))) {
            return true;
        }
        return false;
    }

    @VisibleForTesting
    public boolean shouldShowTabSwitcherToolbar() {
        // Always show in TABSWITCHER
        if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER) return true;

        // Never show on explore pane.
        if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES
                && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) {
            return false;
        }

        // Hide when focusing the Omnibox.
        return (mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)
                        ? mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE)
                        : mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE));
    }

    private void setTabCarouselVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) return;

        mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, isVisible);
    }

    private void setMVTilesVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(MV_TILES_VISIBLE)) return;
        mPropertyModel.set(MV_TILES_VISIBLE, isVisible);
    }

    private void setFakeBoxVisibility(boolean isVisible) {
        if (mPropertyModel == null) return;
        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, isVisible);

        // This is because LocationBarVoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the LocationBarVoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(() -> {
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mFakeboxDelegate.getLocationBarVoiceRecognitionHandler()
                            .isVoiceSearchEnabled());
        });
    }

    private void setIncognitoModeDescriptionVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE)) return;

        if (!mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
            mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
        }
        mPropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
        if (mSecondaryTasksSurfacePropertyModel != null) {
            if (!mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
                mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_INITIALIZED, true);
            }
            mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO_DESCRIPTION_VISIBLE, isVisible);
        }
    }

    @OverviewModeState

    private int computeOverviewStateShown() {
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            if (mOverviewModeState == OverviewModeState.SHOWING_PREVIOUS) {
                assert (isShownState(mPreviousOverviewModeState));
                return mPreviousOverviewModeState;
            } else if (mOverviewModeState == OverviewModeState.SHOWING_START) {
                return OverviewModeState.SHOWN_HOMEPAGE;
            } else if (mOverviewModeState == OverviewModeState.SHOWING_TABSWITCHER) {
                return OverviewModeState.SHOWN_TABSWITCHER;
            } else if (mOverviewModeState == OverviewModeState.SHOWING_HOMEPAGE) {
                return OverviewModeState.SHOWN_HOMEPAGE;
            } else {
                assert (isShownState(mOverviewModeState)
                        || mOverviewModeState == OverviewModeState.NOT_SHOWN);
                return mOverviewModeState;
            }
        }
        if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            return OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES;
        }
        if (mSurfaceMode == SurfaceMode.TASKS_ONLY) {
            return OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY;
        }
        if (mSurfaceMode == SurfaceMode.OMNIBOX_ONLY) {
            return OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY;
        }
        return OverviewModeState.DISABLED;
    }

    private boolean isShownState(@OverviewModeState int state) {
        return state == OverviewModeState.SHOWN_HOMEPAGE
                || state == OverviewModeState.SHOWN_TABSWITCHER
                || state == OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES
                || state == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY
                || state == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY;
    }
}
