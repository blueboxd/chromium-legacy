// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
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
    private final TabGridDialogMediator.ResetHandler mDialogResetHandler;
    private final GridTabSwitcherMediator.ResetHandler mGridTabSwitcherResetHandler;
    private final AnimationOriginProvider mAnimationOriginProvider;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;

    TabGridDialogMediator(Context context, TabGridDialogMediator.ResetHandler dialogResetHandler,
            PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            GridTabSwitcherMediator.ResetHandler gridTabSwitcherResetHandler,
            AnimationOriginProvider animationOriginProvider) {
        mContext = context;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mDialogResetHandler = dialogResetHandler;
        mGridTabSwitcherResetHandler = gridTabSwitcherResetHandler;
        mAnimationOriginProvider = animationOriginProvider;

        // Register for tab model.
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateDialog();
                updateGridTabSwitcher();
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
                    mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);
                    mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                List<Tab> relatedTabs = getRelatedTabs(tab.getId());
                // If current tab is closed and tab group is not empty, hand over ID of the next
                // tab in the group to mCurrentTabId.
                if (relatedTabs.size() == 0) return;
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

    void onReset(Integer tabId) {
        if (tabId != null) {
            mCurrentTabId = tabId;
            TabGroupModelFilter filter =
                    (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter();
            int index = filter.indexOf(
                    TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
            if (mAnimationOriginProvider != null) {
                Rect rect = mAnimationOriginProvider.getAnimationOriginRect(index);
                mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, rect);
            }
            updateDialog();
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        } else {
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

    private void updateGridTabSwitcher() {
        if (!mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE)
                || mGridTabSwitcherResetHandler == null)
            return;
        mGridTabSwitcherResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), false);
    }

    private void updateDialog() {
        if (mCurrentTabId == Tab.INVALID_TAB_ID) return;
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int tabsCount = relatedTabs.size();
        if (tabsCount == 0) {
            mDialogResetHandler.resetWithListOfTabs(null);
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
                mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
            }
            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridSheetProperties.SCRIMVIEW_OBSERVER, scrimObserver);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);
            mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
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
}
