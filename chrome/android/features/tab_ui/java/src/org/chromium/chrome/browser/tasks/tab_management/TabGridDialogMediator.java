// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMediator.INITIAL_SCROLL_INDEX_OFFSET;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
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
         * TabSwitcherMediator}.
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
     * Defines an interface for a {@link TabGridDialogMediator} to get the {@link
     * TabGridDialogParent.AnimationParams} in order to prepare show/hide animation.
     */
    interface AnimationParamsProvider {
        /**
         * Provide a {@link TabGridDialogParent.AnimationParams} to setup the animation.
         *
         * @param tabId The id of the tab whose position is requested.
         * @return A {@link TabGridDialogParent.AnimationParams} used to setup the animation.
         */
        TabGridDialogParent.AnimationParams getAnimationParamsForTab(int tabId);
    }

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;
    private final ResetHandler mDialogResetHandler;
    private final TabSwitcherMediator.ResetHandler mTabSwitcherResetHandler;
    private final AnimationParamsProvider mAnimationParamsProvider;
    private final DialogHandler mTabGridDialogHandler;
    private final String mComponentName;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;

    TabGridDialogMediator(Context context, ResetHandler dialogResetHandler, PropertyModel model,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager,
            TabSwitcherMediator.ResetHandler tabSwitcherResetHandler,
            AnimationParamsProvider animationParamsProvider, String componentName) {
        mContext = context;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mDialogResetHandler = dialogResetHandler;
        mTabSwitcherResetHandler = tabSwitcherResetHandler;
        mAnimationParamsProvider = animationParamsProvider;
        mTabGridDialogHandler = new DialogHandler();
        mComponentName = componentName;

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

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                boolean isIncognito = newModel.isIncognito();
                int dialogBackgroundResource = isIncognito
                        ? R.drawable.tab_grid_dialog_background_incognito
                        : R.drawable.tab_grid_dialog_background;
                ColorStateList tintList = isIncognito
                        ? AppCompatResources.getColorStateList(mContext, R.color.tint_on_dark_bg)
                        : AppCompatResources.getColorStateList(
                                mContext, R.color.standard_mode_tint);
                int ungroupBarBackgroundColorId = isIncognito
                        ? R.color.tab_grid_dialog_background_color_incognito
                        : R.color.tab_grid_dialog_background_color;
                int ungroupBarHoveredBackgroundColorId = isIncognito
                        ? R.color.tab_grid_card_selected_color_incognito
                        : R.color.tab_grid_card_selected_color;
                int ungroupBarTextAppearance = isIncognito
                        ? R.style.TextAppearance_BlueTitle2Incognito
                        : R.style.TextAppearance_BlueTitle2;

                mModel.set(TabGridPanelProperties.DIALOG_BACKGROUND_RESOUCE_ID,
                        dialogBackgroundResource);
                mModel.set(TabGridPanelProperties.TINT, tintList);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID,
                        ungroupBarBackgroundColorId);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID,
                        ungroupBarHoveredBackgroundColorId);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_TEXT_APPEARANCE,
                        ungroupBarTextAppearance);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // Setup toolbar property model.
        setupToolbarClickHandlers();

        // Setup ScrimView observer.
        setupScrimViewObserver();
    }

    void hideDialog(boolean showAnimation) {
        if (!showAnimation) {
            mModel.set(TabGridPanelProperties.ANIMATION_PARAMS, null);
        } else {
            if (mAnimationParamsProvider != null && mCurrentTabId != Tab.INVALID_TAB_ID) {
                mModel.set(TabGridPanelProperties.ANIMATION_PARAMS,
                        mAnimationParamsProvider.getAnimationParamsForTab(mCurrentTabId));
            }
        }
        mDialogResetHandler.resetWithListOfTabs(null);
    }

    void onReset(@Nullable List<Tab> tabs) {
        if (tabs == null) {
            mCurrentTabId = Tab.INVALID_TAB_ID;
        } else {
            TabModelFilter filter =
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
            mCurrentTabId = filter.getTabAt(filter.indexOf(tabs.get(0))).getId();
        }

        if (mCurrentTabId != Tab.INVALID_TAB_ID) {
            if (mAnimationParamsProvider != null) {
                TabGridDialogParent.AnimationParams params =
                        mAnimationParamsProvider.getAnimationParamsForTab(mCurrentTabId);
                mModel.set(TabGridPanelProperties.ANIMATION_PARAMS, params);
            }
            updateDialog();
            updateDialogScrollPosition();
            mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        } else {
            mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
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
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    boolean isVisible() {
        return mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE);
    }

    private void updateGridTabSwitcher() {
        if (!isVisible() || mTabSwitcherResetHandler == null) return;
        mTabSwitcherResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), false,
                false);
    }

    private void updateDialog() {
        if (mCurrentTabId == Tab.INVALID_TAB_ID) return;
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int tabsCount = relatedTabs.size();
        if (tabsCount == 0) {
            hideDialog(true);
            return;
        }
        mModel.set(TabGridPanelProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount));
    }

    private void updateDialogScrollPosition() {
        // If current selected tab is not within this dialog, always scroll to the top.
        if (mCurrentTabId != mTabModelSelector.getCurrentTabId()) {
            mModel.set(TabGridPanelProperties.INITIAL_SCROLL_INDEX, 0);
            return;
        }
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
        int initialPosition =
                Math.max(relatedTabs.indexOf(currentTab) - INITIAL_SCROLL_INDEX_OFFSET, 0);
        mModel.set(TabGridPanelProperties.INITIAL_SCROLL_INDEX, initialPosition);
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridPanelProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridPanelProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private void setupScrimViewObserver() {
        ScrimView.ScrimObserver scrimObserver = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                hideDialog(true);
                RecordUserAction.record("TabGridDialog.Exit");
            }
            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridPanelProperties.SCRIMVIEW_OBSERVER, scrimObserver);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            hideDialog(true);
            RecordUserAction.record("TabGridDialog.Exit");
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            // Get the current Tab first since hideDialog causes mCurrentTabId to be
            // Tab.INVALID_TAB_ID.
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
            hideDialog(false);
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
            RecordUserAction.record("MobileNewTabOpened." + mComponentName);
        };
    }

    private View.OnClickListener getMenuButtonClickListener() {
        return TabGridDialogMenuCoordinator.getTabGridDialogMenuOnClickListener(null);
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
            mModel.set(TabGridPanelProperties.UNGROUP_BAR_STATUS, status);
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
