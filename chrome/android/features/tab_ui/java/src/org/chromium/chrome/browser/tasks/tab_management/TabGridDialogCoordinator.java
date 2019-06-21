// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGridDialog component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
public class TabGridDialogCoordinator {
    final static String COMPONENT_NAME = "TabGridDialog";
    private final Context mContext;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mToolbarPropertyModel;
    private TabGridSheetToolbarCoordinator mToolbarCoordinator;
    private TabGridDialogParent mParentLayout;

    TabGridDialogCoordinator(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            CompositorViewHolder compositorViewHolder,
            GridTabSwitcherMediator.ResetHandler resetHandler,
            TabGridDialogMediator.AnimationOriginProvider animationOriginProvider) {
        mContext = context;

        mToolbarPropertyModel = new PropertyModel(TabGridSheetProperties.ALL_KEYS);

        mMediator =
                new TabGridDialogMediator(context, this::resetWithListOfTabs, mToolbarPropertyModel,
                        tabModelSelector, tabCreatorManager, resetHandler, animationOriginProvider);

        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false, null,
                null, mMediator.getTabGridDialogHandler(), null, null, compositorViewHolder, null,
                false, R.layout.tab_list_recycler_view_layout, COMPONENT_NAME);

        mParentLayout = new TabGridDialogParent(context, compositorViewHolder);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mMediator.destroy();
        mParentLayout.destroy();
    }

    private void updateDialogContent(List<Tab> tabs) {
        if (tabs != null) {
            TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();
            mToolbarCoordinator = new TabGridSheetToolbarCoordinator(
                    mContext, recyclerView, mToolbarPropertyModel, mParentLayout);
            mMediator.onReset(tabs.get(0).getId());
        } else {
            mMediator.onReset(null);
            if (mToolbarCoordinator != null) {
                mToolbarCoordinator.destroy();
            }
        }
    }

    TabGridDialogMediator.ResetHandler getResetHandler() {
        return this::resetWithListOfTabs;
    }

    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
        updateDialogContent(tabs);
    }
}
