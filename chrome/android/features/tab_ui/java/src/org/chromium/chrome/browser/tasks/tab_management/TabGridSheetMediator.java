// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View.OnClickListener;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.tab_ui.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGridSheet component, responsible for communicating
 * with the components' coordinator as well as managing the state of the bottom
 * sheet.
 */
class TabGridSheetMediator {
    /**
     * Defines an interface for a {@link TabGridSheetMediator} reset event handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGridSheetMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);
    }

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mSheetObserver;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final ThemeColorProvider.ThemeColorObserver mThemeColorObserver;
    private final ThemeColorProvider.TintObserver mTintObserver;
    private final ResetHandler mResetHandler;

    TabGridSheetMediator(Context context, BottomSheetController bottomSheetController,
            ResetHandler resetHandler, PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager, ThemeColorProvider themeColorProvider) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mThemeColorProvider = themeColorProvider;
        mResetHandler = resetHandler;

        // TODO (ayman): Add instrumentation to observer calls.
        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.TAP_SCRIM) {
                    RecordUserAction.record("TabGroup.MinimizedFromGrid");
                }
                resetHandler.resetWithListOfTabs(null);
            }
        };

        // register for tab model
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateBottomSheet();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateBottomSheet();
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_USER) resetHandler.resetWithListOfTabs(null);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                updateBottomSheet();
            }
        };
        mThemeColorObserver =
                (color, shouldAnimate) -> mModel.set(TabGridPanelProperties.PRIMARY_COLOR, color);
        mTintObserver = (tint, useLight) -> mModel.set(TabGridPanelProperties.TINT, tint);

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        mThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.addTintObserver(mTintObserver);
        mModel.set(TabGridPanelProperties.IS_MAIN_CONTENT_VISIBLE, true);

        // setup toolbar property model
        setupToolbarClickHandlers();
    }

    /**
     * Handles communication with the bottom sheet based on the content provided.
     *
     * @param sheetContent The {@link TabGridSheetContent} to populate the
     *                     bottom sheet with. When set to null, the bottom sheet
     *                     will be hidden.
     */
    void onReset(TabGridSheetContent sheetContent) {
        if (sheetContent == null) {
            hideTabGridSheet();
        } else {
            showTabGridSheet(sheetContent);
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
        mThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.removeTintObserver(mTintObserver);
    }

    private void showTabGridSheet(TabGridSheetContent sheetContent) {
        updateBottomSheet();
        mBottomSheetController.getBottomSheet().addObserver(mSheetObserver);
        mBottomSheetController.requestShowContent(sheetContent, true);
        mBottomSheetController.expandSheet();
    }

    private void hideTabGridSheet() {
        mBottomSheetController.hideContent(getCurrentSheetContent(), true);
        mBottomSheetController.getBottomSheet().removeObserver(mSheetObserver);
    }

    private BottomSheetContent getCurrentSheetContent() {
        return mBottomSheetController.getBottomSheet() != null
                ? mBottomSheetController.getBottomSheet().getCurrentSheetContent()
                : null;
    }

    private void updateBottomSheet() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        int tabsCount = mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .getRelatedTabList(currentTab.getId())
                                .size();
        if (tabsCount == 0) {
            mResetHandler.resetWithListOfTabs(null);
            return;
        }

        mModel.set(TabGridPanelProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount));
        mModel.set(TabGridPanelProperties.CONTENT_TOP_MARGIN,
                (int) mContext.getResources().getDimension(R.dimen.control_container_height));
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridPanelProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridPanelProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private OnClickListener getCollapseButtonClickListener() {
        return view -> {
            RecordUserAction.record("TabGroup.MinimizedFromGrid");
            hideTabGridSheet();
        };
    }

    private OnClickListener getAddButtonClickListener() {
        return view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            List<Tab> relatedTabs = mTabModelSelector.getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()
                                            .getRelatedTabList(currentTab.getId());

            assert relatedTabs.size() > 0;

            Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_CHROME_UI, parentTabToAttach);
            RecordUserAction.record("MobileNewTabOpened." + TabGridSheetCoordinator.COMPONENT_NAME);
        };
    }
}
