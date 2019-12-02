// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
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
            SurfaceMode.SINGLE_PANE})
    @Retention(RetentionPolicy.SOURCE)
    @interface SurfaceMode {
        int NO_START_SURFACE = 0;
        int TASKS_ONLY = 1;
        int TWO_PANES = 2;
        int SINGLE_PANE = 3;
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
    private boolean mIsOmniboxFocused;
    @Nullable
    private TabModel mNormalTabModel;
    @Nullable
    private TabModelObserver mNormalTabModelObserver;
    @Nullable
    private TabModelSelectorObserver mTabModelSelectorObserver;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable ExploreSurfaceCoordinator.FeedSurfaceCreator feedSurfaceCreator,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            @SurfaceMode int surfaceMode, @Nullable FakeboxDelegate fakeboxDelegate,
            NightModeStateProvider nightModeStateProvider) {
        mController = controller;
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mFeedSurfaceCreator = feedSurfaceCreator;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mSurfaceMode = surfaceMode;
        mFakeboxDelegate = fakeboxDelegate;
        mNightModeStateProvider = nightModeStateProvider;

        if (mPropertyModel != null) {
            assert mSurfaceMode == SurfaceMode.SINGLE_PANE || mSurfaceMode == SurfaceMode.TWO_PANES
                    || mSurfaceMode == SurfaceMode.TASKS_ONLY;
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
                        if (mNormalTabModel.getCount() <= 1) {
                            setTabCarouselVisibility(false);
                        }
                    }
                    @Override
                    public void tabClosureUndone(Tab tab) {
                        setTabCarouselVisibility(true);
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
                    // No fake search box on the explore pane in two panes mode.
                    if (mSurfaceMode != SurfaceMode.TWO_PANES
                            || !mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) {
                        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, !hasFocus);
                    }
                    if (mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
                        mSecondaryTasksSurfacePropertyModel.set(
                                IS_FAKE_SEARCH_BOX_VISIBLE, !hasFocus);
                    }
                    notifyStateChange();
                }
            };
        }
        mController.addOverviewModeObserver(this);
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

    @VisibleForTesting
    public void setOverviewState(@OverviewModeState int state) {
        if (state == mOverviewModeState) return;

        mOverviewModeState = state;
        setOverviewStateInternal(mOverviewModeState);
        notifyStateChange();
    }

    private void setOverviewStateInternal(@OverviewModeState int newState) {
        if (newState == OverviewModeState.SHOWN_HOMEPAGE) {
            RecordUserAction.record("StartSurface.SinglePane");
            setExploreSurfaceVisibility(!mIsIncognito);
            setTabCarouselVisibility(
                    mTabModelSelector.getModel(false).getCount() > 0 && !mIsIncognito);
            setMVTilesVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(mIsIncognito);
            mNormalTabModel.addObserver(mNormalTabModelObserver);

        } else if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER) {
            setExploreSurfaceVisibility(false);
            setTabCarouselVisibility(false);
            setMVTilesVisibility(false);
            setSecondaryTasksSurfaceVisibility(true);
        } else if (mOverviewModeState == OverviewModeState.SHOWN_TWO_PANES) {
            RecordUserAction.record("StartSurface.TwoPanes");
            String defaultOnUserActionString = mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    ? "ExploreSurface"
                    : "HomeSurface";
            RecordUserAction.record("StartSurface.TwoPanes.DefaultOn" + defaultOnUserActionString);

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

        } else if (mOverviewModeState == OverviewModeState.SHOWN_TASKS_ONLY) {
            RecordUserAction.record("StartSurface.TasksOnly");
            setMVTilesVisibility(!mIsIncognito);
            setExploreSurfaceVisibility(false);
        } else if (mOverviewModeState == OverviewModeState.NOT_SHOWN) {
            if (mSecondaryTasksSurfaceController != null) setSecondaryTasksSurfaceVisibility(false);
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
        setOverviewState(OverviewModeState.NOT_SHOWN);
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            int state = computeOverviewStateShown();

            // update incognito
            mIsIncognito = mTabModelSelector.isIncognitoSelected();
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            setOverviewState(state);

            // Make sure FeedSurfaceCoordinator is built before the explore surface is showing by
            // default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
                mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                        mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                                mNightModeStateProvider.isInNightMode()));
            }
            mTabModelSelector.addObserver(mTabModelSelectorObserver);

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
            // TODO: differentiate between "more tabs" and tabswitcher
            setOverviewState(OverviewModeState.SHOWN_HOMEPAGE);
            return true;
        }

        if (mPropertyModel != null && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mOverviewModeState == OverviewModeState.SHOWN_TWO_PANES) {
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
        setOverviewState(OverviewModeState.NOT_SHOWN);
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

        setOverviewState(OverviewModeState.SHOWN_TABSWITCHER);
        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                    mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                            mNightModeStateProvider.isInNightMode()));
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        if (mOverviewModeState == OverviewModeState.SHOWN_TWO_PANES) {
            // Update the 'BOTTOM_BAR_SELECTED_TAB_POSITION' property to reflect the change. This is
            // needed when clicking back button on the explore surface.
            mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, isVisible ? 1 : 0);
            ReturnToStartSurfaceUtil.setExploreSurfaceVisibleLast(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        // This is because LocationBarVoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the LocationBarVoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(() -> {
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mFakeboxDelegate.getLocationBarVoiceRecognitionHandler()
                            .isVoiceSearchEnabled());
        });

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        setOverviewStateInternal(mOverviewModeState);

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
            mSecondaryTasksSurfaceController.showOverview(false);
        } else {
            if (mSecondaryTasksSurfaceController == null) return;
            mSecondaryTasksSurfaceController.hideOverview(false);
        }
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, isVisible);
    }

    private void notifyStateChange() {
        if (mStateObserver != null) {
            mStateObserver.onStateChanged(shouldShowTabSwitcherToolbar());
        }
    }

    private boolean shouldShowTabSwitcherToolbar() {
        // Do not show Tab switcher toolbar on explore pane.
        if (mOverviewModeState == OverviewModeState.SHOWN_TWO_PANES
                && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) {
            return false;
        }

        // Do not show Tab switcher toolbar when focusing the Omnibox.
        return mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE);
    }

    private void setTabCarouselVisibility(boolean isVisible) {

        if (isVisible == mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) return;

        // Hide the more Tabs view when the last Tab is closed.
        if (!isVisible && mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()) {
            setSecondaryTasksSurfaceVisibility(false);
            setExploreSurfaceVisibility(true);
        }

        mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, isVisible);
    }

    private void setMVTilesVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(MV_TILES_VISIBLE)) return;
        mPropertyModel.set(MV_TILES_VISIBLE, isVisible);
    }

    @OverviewModeState
    public int computeOverviewStateShown() {
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) return OverviewModeState.SHOWN_HOMEPAGE;
        if (mSurfaceMode == SurfaceMode.TWO_PANES) return OverviewModeState.SHOWN_TWO_PANES;
        if (mSurfaceMode == SurfaceMode.TASKS_ONLY) return OverviewModeState.SHOWN_TASKS_ONLY;
        return OverviewModeState.DISABLED;
    }
}
