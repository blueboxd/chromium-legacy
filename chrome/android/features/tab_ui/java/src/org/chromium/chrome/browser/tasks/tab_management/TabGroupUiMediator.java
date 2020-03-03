// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Handler;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGroupUi. Responsible for managing the
 * internal state of the component.
 */
public class TabGroupUiMediator {
    /**
     * An interface to control the TabGroupUi component.
     */
    interface TabGroupUiController {
        /**
         * Setup the drawable in TabGroupUi left button with a drawable ID.
         * @param drawableId Resource ID of the drawable to setup the left button.
         */
        void setupLeftButtonDrawable(int drawableId);

        /**
         * Setup the {@link View.OnClickListener} of the left button in TabGroupUi.
         * @param listener {@link View.OnClickListener} to setup the left button.
         */
        void setupLeftButtonOnClickListener(View.OnClickListener listener);
    }

    /**
     * Defines an interface for a {@link TabGroupUiMediator} reset event
     * handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is collapsed or the dialog is hidden.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetStripWithListOfTabs(List<Tab> tabs);

        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is expanded or the dialog is shown.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetGridWithListOfTabs(List<Tab> tabs);
    }

    private final PropertyModel mModel;
    private final TabModelObserver mTabModelObserver;
    private final ResetHandler mResetHandler;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private final BottomControlsCoordinator
            .BottomControlsVisibilityController mVisibilityController;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabGridDialogMediator.DialogController mTabGridDialogController;
    private final ThemeColorProvider.ThemeColorObserver mThemeColorObserver;
    private final ThemeColorProvider.TintObserver mTintObserver;
    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final TabGroupModelFilter.Observer mTabGroupModelFilterObserver;
    private boolean mIsTabGroupUiVisible;
    private boolean mIsShowingOverViewMode;

    TabGroupUiMediator(
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            ResetHandler resetHandler, PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager, OverviewModeBehavior overviewModeBehavior,
            ThemeColorProvider themeColorProvider,
            @Nullable TabGridDialogMediator.DialogController dialogController) {
        mResetHandler = resetHandler;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mOverviewModeBehavior = overviewModeBehavior;
        mVisibilityController = visibilityController;
        mThemeColorProvider = themeColorProvider;
        mTabGridDialogController = dialogController;

        // register for tab model
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (type == TabSelectionType.FROM_CLOSE) return;
                if (getRelatedTabsForId(lastId).contains(tab)) return;
                // TODO(995956): Optimization we can do here if we decided always hide the strip if
                // related tab size down to 1.
                resetTabStripWithRelatedTabsForId(tab.getId());
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (!mIsTabGroupUiVisible) return;
                List<Tab> group = mTabModelSelector.getTabModelFilterProvider()
                                          .getCurrentTabModelFilter()
                                          .getRelatedTabList(tab.getId());

                if (group.size() == 1) resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
            }

            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState) {
                if (type == TabLaunchType.FROM_CHROME_UI && mIsTabGroupUiVisible) {
                    mModel.set(TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                            getRelatedTabsForId(tab.getId()).size() - 1);
                }
                if (type == TabLaunchType.FROM_CHROME_UI || type == TabLaunchType.FROM_RESTORE
                        || type == TabLaunchType.FROM_STARTUP) {
                    return;
                }
                resetTabStripWithRelatedTabsForId(tab.getId());
            }

            @Override
            public void restoreCompleted() {
                Tab currentTab = mTabModelSelector.getCurrentTab();
                // Do not try to show tab strip when there is no current tab or we are not in tab
                // page when restore completed.
                if (currentTab == null || overviewModeBehavior.overviewVisible()) return;
                resetTabStripWithRelatedTabsForId(currentTab.getId());
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (!mIsTabGroupUiVisible && !mIsShowingOverViewMode) {
                    resetTabStripWithRelatedTabsForId(tab.getId());
                }
            }
        };
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mIsShowingOverViewMode = true;
                resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mIsShowingOverViewMode = false;
                Tab tab = mTabModelSelector.getCurrentTab();
                if (tab == null) return;
                resetTabStripWithRelatedTabsForId(tab.getId());
            }
        };

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                List<Tab> listOfTabs = mTabModelSelector.getTabModelFilterProvider()
                                               .getCurrentTabModelFilter()
                                               .getRelatedTabList(tab.getId());
                int numTabs = listOfTabs.size();
                // This is set to zero because the UI is hidden.
                if (!mIsTabGroupUiVisible || numTabs == 1) numTabs = 0;
                RecordHistogram.recordCountHistogram("TabStrip.TabCountOnPageLoad", numTabs);
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                if (newModel.isIncognito() && newModel.getCount() == 1) {
                    resetTabStripWithRelatedTabsForId(newModel.getTabAt(0).getId());
                }
            }
        };

        mTabGroupModelFilterObserver = new EmptyTabGroupModelFilterObserver() {
            @Override
            public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                if (mIsTabGroupUiVisible && movedTab == mTabModelSelector.getCurrentTab()) {
                    resetTabStripWithRelatedTabsForId(movedTab.getId());
                }
            }
        };

        // TODO(995951): Add observer similar to TabModelSelectorTabModelObserver for
        // TabModelFilter.
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 false))
                .addTabGroupObserver(mTabGroupModelFilterObserver);
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 true))
                .addTabGroupObserver(mTabGroupModelFilterObserver);

        mThemeColorObserver =
                (color, shouldAnimate) -> mModel.set(TabGroupUiProperties.PRIMARY_COLOR, color);
        mTintObserver = (tint, useLight) -> mModel.set(TabGroupUiProperties.TINT, tint);

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        mThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.addTintObserver(mTintObserver);

        setupToolbarClickHandlers();
        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            resetTabStripWithRelatedTabsForId(tab.getId());
        }
    }

    void setupLeftButtonDrawable(int drawableId) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, drawableId);
    }

    void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, listener);
    }

    private void setupToolbarClickHandlers() {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            if (currentTab == null) return;
            mResetHandler.resetGridWithListOfTabs(getRelatedTabsForId(currentTab.getId()));
            RecordUserAction.record("TabGroup.ExpandedFromStrip.TabGridDialog");
        });
        mModel.set(
                TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER, view -> {
                    Tab currentTab = mTabModelSelector.getCurrentTab();
                    List<Tab> relatedTabs = mTabModelSelector.getTabModelFilterProvider()
                                                    .getCurrentTabModelFilter()
                                                    .getRelatedTabList(currentTab.getId());

                    assert relatedTabs.size() > 0;

                    Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
                    mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                            .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                                    TabLaunchType.FROM_CHROME_UI, parentTabToAttach);
                    RecordUserAction.record(
                            "MobileNewTabOpened." + TabGroupUiCoordinator.COMPONENT_NAME);
                });
    }

    private void resetTabStripWithRelatedTabsForId(int id) {
        List<Tab> listOfTabs = mTabModelSelector.getTabModelFilterProvider()
                                       .getCurrentTabModelFilter()
                                       .getRelatedTabList(id);
        if (listOfTabs.size() < 2) {
            mResetHandler.resetStripWithListOfTabs(null);
            mIsTabGroupUiVisible = false;
        } else {
            mResetHandler.resetStripWithListOfTabs(listOfTabs);
            mIsTabGroupUiVisible = true;
        }
        boolean isDuetTabStripIntegrationEnabled =
                TabUiFeatureUtilities.isDuetTabStripIntegrationAndroidEnabled()
                && BottomToolbarConfiguration.isBottomToolbarEnabled();
        assert (mVisibilityController == null) == isDuetTabStripIntegrationEnabled;
        if (isDuetTabStripIntegrationEnabled) return;
        if (mIsTabGroupUiVisible) {
            // Post to make sure that the recyclerView already knows how many visible items it has.
            // This is to make sure that we can scroll to a state where the selected tab is in the
            // middle of the strip.
            Handler handler = new Handler();
            handler.post(()
                                 -> mModel.set(TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                                         listOfTabs.indexOf(mTabModelSelector.getCurrentTab())));
        }
        mVisibilityController.setBottomControlsVisible(mIsTabGroupUiVisible);
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    public boolean onBackPressed() {
        // TODO(crbug.com/1006421): add a regression test to make sure that the back button closes
        // the dialog when the dialog is showing.
        return mTabGridDialogController != null && mTabGridDialogController.handleBackPressed();
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     false))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     true))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        mThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.removeTintObserver(mTintObserver);
        mTabModelSelectorTabObserver.destroy();
    }

    @VisibleForTesting
    boolean getIsShowingOverViewModeForTesting() {
        return mIsShowingOverViewMode;
    }
}
