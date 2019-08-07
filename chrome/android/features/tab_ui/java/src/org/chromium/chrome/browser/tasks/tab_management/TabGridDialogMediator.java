// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGridDialog component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for dialog show/hide.
 */
public class TabGridDialogMediator {
    /**
     * Defines an interface for a {@link TabGridDialogMediator} reset event handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGridDialogMediator} and {@link
         * GridTabSwitcherMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);

        /**
         * Hide the TabGridDialog
         * @param showAnimation Whether to show an animation when hiding the dialog.
         */
        void hideDialog(boolean showAnimation);
    }

    /**
     * Defines an interface for a {@link TabGridDialogMediator} to get the position on the screen
     * of the tab grid card related to TabGridDialog in order to prepare animation.
     */
    interface AnimationOriginProvider {
        /**
         * Provide position of a tab card in GridTabSwitcher as the originate position of a
         * animation.
         *
         * @param index Index in GridTabSwitcher of the tab whose position is requested.
         * @return  A {@link Rect} that contains position information of the tab card.
         */
        Rect getAnimationOriginRect(int index);
    }

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;
    private final ResetHandler mDialogResetHandler;
    private final TabSwitcherMediator.ResetHandler mTabSwitcherResetHandler;
    private final AnimationOriginProvider mAnimationOriginProvider;
    private final DialogHandler mTabGridDialogHandler;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;

    TabGridDialogMediator(Context context, ResetHandler dialogResetHandler, PropertyModel model,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager,
            TabSwitcherMediator.ResetHandler tabSwitcherResetHandler,
            AnimationOriginProvider animationOriginProvider) {
        mContext = context;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mDialogResetHandler = dialogResetHandler;
        mTabSwitcherResetHandler = tabSwitcherResetHandler;
        mAnimationOriginProvider = animationOriginProvider;
        mTabGridDialogHandler = new DialogHandler();

        // Register for tab model.
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                hideDialog(false);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateDialog();
                updateGridTabSwitcher();
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_USER) {
                    // Cancel the zooming into tab grid card animation.
                    hideDialog(false);
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                List<Tab> relatedTabs = getRelatedTabs(tab.getId());
                // If the group is empty, update the animation and hide the dialog.
                if (relatedTabs.size() == 0) {
                    mCurrentTabId = Tab.INVALID_TAB_ID;
                    hideDialog(false);
                    return;
                }
                // If current tab is closed and tab group is not empty, hand over ID of the next
                // tab in the group to mCurrentTabId.
                if (tab.getId() == mCurrentTabId) {
                    mCurrentTabId = relatedTabs.get(0).getId();
                }
                updateDialog();
                updateGridTabSwitcher();
            }
        };
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        // Setup toolbar property model.
        setupToolbarClickHandlers();

        // Setup ScrimView observer.
        setupScrimViewObserver();
    }

    void hideDialog(boolean showAnimation) {
        if (!showAnimation) mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);
        mDialogResetHandler.resetWithListOfTabs(null);
    }

    void onReset(Integer tabId) {
        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        if (tabId != null) {
            mCurrentTabId = tabId;
            int index = filter.indexOf(
                    TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
            if (mAnimationOriginProvider != null) {
                Rect rect = mAnimationOriginProvider.getAnimationOriginRect(index);
                mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, rect);
            }
            updateDialog();
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        } else {
            if (!FeatureUtilities.isTabToGtsAnimationEnabled()) {
                int index = filter.indexOf(TabModelUtils.getTabById(
                        mTabModelSelector.getCurrentModel(), mCurrentTabId));
                if (mAnimationOriginProvider != null && index != TabModel.INVALID_TAB_INDEX) {
                    Rect rect = mAnimationOriginProvider.getAnimationOriginRect(index);
                    mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, rect);
                }
            }
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
    }

    boolean isVisible() {
        return mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE);
    }

    private void updateGridTabSwitcher() {
        if (!isVisible() || mTabSwitcherResetHandler == null) return;
        mTabSwitcherResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), false);
    }

    private void updateDialog() {
        if (mCurrentTabId == Tab.INVALID_TAB_ID) return;
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int tabsCount = relatedTabs.size();
        if (tabsCount == 0) {
            hideDialog(true);
            return;
        }
        mModel.set(TabGridSheetProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        org.chromium.chrome.R.plurals.bottom_tab_grid_title_placeholder, tabsCount,
                        tabsCount));
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridSheetProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridSheetProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private void setupScrimViewObserver() {
        ScrimView.ScrimObserver scrimObserver = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                hideDialog(true);
            }
            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridSheetProperties.SCRIMVIEW_OBSERVER, scrimObserver);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            hideDialog(true);
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            hideDialog(false);
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
            if (currentTab == null) {
                mTabCreatorManager.getTabCreator(mTabModelSelector.isIncognitoSelected())
                        .launchNTP();
                return;
            }
            List<Tab> relatedTabs = getRelatedTabs(currentTab.getId());

            assert relatedTabs.size() > 0;

            Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_CHROME_UI, parentTabToAttach);
        };
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }

    TabListMediator.TabGridDialogHandler getTabGridDialogHandler() {
        return mTabGridDialogHandler;
    }

    /**
     * A handler that handles TabGridDialog related changes originated from {@link TabListMediator}
     * and {@link TabGridItemTouchHelperCallback}.
     */
    class DialogHandler implements TabListMediator.TabGridDialogHandler {
        @Override
        public void updateUngroupBarStatus(@TabGridDialogParent.UngroupBarStatus int status) {
            mModel.set(TabGridSheetProperties.UNGROUP_BAR_STATUS, status);
        }

        @Override
        public void updateDialogContent(int tabId) {
            mCurrentTabId = tabId;
            updateDialog();
        }
    }

    @VisibleForTesting
    int getCurrentTabIdForTest() {
        return mCurrentTabId;
    }

    @VisibleForTesting
    void setCurrentTabIdForTest(int tabId) {
        mCurrentTabId = tabId;
    }
}
