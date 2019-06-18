// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.MenuOrKeyboardActionController;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationLayout;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid of tabs for the main TabSwitcher UI.
 */
public class GridTabSwitcherCoordinator
        implements Destroyable, GridTabSwitcher, GridTabSwitcherMediator.ResetHandler {
    final static String COMPONENT_NAME = "GridTabSwitcher";
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabGridCoordinator;
    private final GridTabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;

    private final MenuOrKeyboardActionController
            .MenuOrKeyboardActionHandler mGridTabSwitcherMenuActionHandler =
            new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                @Override
                public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_group_tabs) {
                        mTabSelectionEditorCoordinator.getController().show();
                        return true;
                    }
                    return false;
                }
            };

    public GridTabSwitcherCoordinator(Context context,
            ActivityLifecycleDispatcher lifecycleDispatcher, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, CompositorViewHolder compositorViewHolder,
            ChromeFullscreenManager fullscreenManager, TabCreatorManager tabCreatorManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController, Runnable backPress) {
        PropertyModel containerViewModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);
        TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider;
        if (FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(context, tabModelSelector,
                    tabContentManager, tabCreatorManager, new CompositorViewHolder(context), this,
                    this::getTabGridCardPosition);

            mMediator = new GridTabSwitcherMediator(this, containerViewModel, tabModelSelector,
                    fullscreenManager, compositorViewHolder,
                    mTabGridDialogCoordinator.getResetHandler());

            gridCardOnClickListenerProvider = mMediator::getGridCardOnClickListener;
        } else {
            mTabGridDialogCoordinator = null;

            mMediator = new GridTabSwitcherMediator(this, containerViewModel, tabModelSelector,
                    fullscreenManager, compositorViewHolder, null);

            gridCardOnClickListenerProvider = null;
        }

        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(context, tabContentManager, tabModelSelector);

        TabListMediator.TitleProvider titleProvider = tab -> {
            int numRelatedTabs = tabModelSelector.getTabModelFilterProvider()
                                         .getCurrentTabModelFilter()
                                         .getRelatedTabList(tab.getId())
                                         .size();
            if (numRelatedTabs == 1) return tab.getTitle();
            return context.getResources().getQuantityString(
                    R.plurals.bottom_tab_grid_title_placeholder, numRelatedTabs, numRelatedTabs);
        };

        mTabGridCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, mMultiThumbnailCardProvider, titleProvider, true,
                mMediator::getCreateGroupButtonOnClickListener, gridCardOnClickListenerProvider,
                null, compositorViewHolder, compositorViewHolder.getDynamicResourceLoader(), true,
                R.layout.grid_tab_switcher_layout, COMPONENT_NAME);

        HistoryNavigationLayout navigation =
                compositorViewHolder.findViewById(R.id.history_navigation);

        navigation.setNavigationDelegate(HistoryNavigationDelegate.createForTabSwitcher(
                context, backPress, tabModelSelector::getCurrentTab));
        mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                mTabGridCoordinator.getContainerView(), TabGridContainerViewBinder::bind);

        mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                context, compositorViewHolder, tabModelSelector, tabContentManager);

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                mGridTabSwitcherMenuActionHandler);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    // GridTabSwitcher implementation.
    @Override
    public GridController getGridController() {
        return mMediator;
    }

    @Override
    public boolean prepareOverview() {
        boolean quick = mMediator.prepareOverview();
        mTabGridCoordinator.prepareOverview();
        return quick;
    }

    @Override
    public void postHiding() {
        mTabGridCoordinator.postHiding();
        mMediator.postHiding();
    }

    @Override
    @NonNull
    public Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate) {
        if (forceUpdate) mTabGridCoordinator.updateThumbnailLocation();
        return mTabGridCoordinator.getThumbnailLocationOfCurrentTab();
    }

    @Override
    public int getResourceId() {
        return mTabGridCoordinator.getResourceId();
    }

    @Override
    public long getLastDirtyTimeForTesting() {
        return mTabGridCoordinator.getLastDirtyTimeForTesting();
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        List<Tab> tabs = null;
        if (tabList != null) {
            tabs = new ArrayList<>();
            for (int i = 0; i < tabList.getCount(); i++) {
                tabs.add(tabList.getTabAt(i));
            }
        }
        return mTabGridCoordinator.resetWithListOfTabs(tabs, quickMode);
    }

    private Rect getTabGridCardPosition(int index) {
        return mTabGridCoordinator.getContainerView().getTabPosition(index);
    }

    @Override
    public void softCleanup() {
        mTabGridCoordinator.softCleanup();
    }

    // ResetHandler implementation.
    @Override
    public void destroy() {
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                mGridTabSwitcherMenuActionHandler);
        mTabGridCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
    }
}
