// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGridSheet component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
public class TabGridSheetCoordinator {
    final static String COMPONENT_NAME = "TabGridSheet";
    private final Context mContext;
    private final TabListCoordinator mTabGridCoordinator;
    private final TabGridSheetMediator mMediator;
    private TabGridSheetContent mBottomSheetContent;
    private TabGridSheetToolbarCoordinator mToolbarCoordinator;
    private final PropertyModel mToolbarPropertyModel;

    TabGridSheetCoordinator(Context context, BottomSheetController bottomSheetController,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager, ThemeColorProvider themeColorProvider) {
        mContext = context;

        mToolbarPropertyModel = new PropertyModel(TabGridSheetProperties.ALL_KEYS);

        mTabGridCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false, null,
                null, null, null, null, bottomSheetController.getBottomSheet(), null, false,
                COMPONENT_NAME);

        mMediator = new TabGridSheetMediator(mContext, bottomSheetController,
                this::resetWithListOfTabs, mToolbarPropertyModel, tabModelSelector,
                tabCreatorManager, themeColorProvider);
        startObservingForCreationIPH();
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabGridCoordinator.destroy();
        mMediator.destroy();

        if (mBottomSheetContent != null) {
            mBottomSheetContent.destroy();
        }

        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
    }

    /**
     * Updates tabs list through {@link TabListCoordinator} with given tab model and
     * calls onReset() on {@link TabGridSheetMediator}
     */
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabGridCoordinator.resetWithListOfTabs(tabs);
        updateBottomSheetContent(tabs);
        mMediator.onReset(mBottomSheetContent);
    }

    private void updateBottomSheetContent(@Nullable List<Tab> tabs) {
        if (tabs != null) {
            // create bottom sheet content
            mToolbarCoordinator = new TabGridSheetToolbarCoordinator(
                    mContext, mTabGridCoordinator.getContainerView(), mToolbarPropertyModel);
            mBottomSheetContent = new TabGridSheetContent(
                    mTabGridCoordinator.getContainerView(), mToolbarCoordinator.getView());
        } else {
            if (mBottomSheetContent != null) {
                mBottomSheetContent.destroy();
                mBottomSheetContent = null;
            }

            if (mToolbarCoordinator != null) {
                mToolbarCoordinator.destroy();
                mToolbarCoordinator = null;
            }
        }
    }

    private void startObservingForCreationIPH() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return;

        TabGroupUtils.startObservingForTabGroupsIPH(
                ((ChromeTabbedActivity) activity).getTabModelSelector());
    }
}
