// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * This class is a coordinator for TabSelectionEditor component. It manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component.
 */
class TabSelectionEditorCoordinator {
    static final String COMPONENT_NAME = "TabSelectionEditor";

    /**
     * An interface to control the TabSelectionEditor.
     */
    interface TabSelectionEditorController {
        /**
         * Shows the TabSelectionEditor.
         */
        void show();

        /**
         * @return Whether or not the TabSelectionEditor consumed the event.
         */
        boolean handleBackPressed();
    }

    private final Context mContext;
    private final View mParentView;
    private final TabModelSelector mTabModelSelector;
    private final TabSelectionEditorLayout mTabSelectionEditorLayout;
    private final TabListCoordinator mTabListCoordinator;
    private final SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();
    private final PropertyModel mModel = new PropertyModel(TabSelectionEditorProperties.ALL_KEYS);
    private final PropertyModelChangeProcessor mTabSelectionEditorLayoutChangeProcessor;
    private final TabSelectionEditorMediator mTabSelectionEditorMediator;

    public TabSelectionEditorCoordinator(Context context, View parentView,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager) {
        mContext = context;
        mParentView = parentView;
        mTabModelSelector = tabModelSelector;
        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                mTabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false,
                null, null, null, this::getItemViewType, this::getSelectionDelegate, null, null,
                false, COMPONENT_NAME);

        mTabSelectionEditorLayout = LayoutInflater.from(context)
                .inflate(R.layout.tab_selection_editor_layout, null)
                .findViewById(R.id.selectable_list);
        mTabSelectionEditorLayout.initialize(mParentView, mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(), mSelectionDelegate);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        mTabSelectionEditorLayoutChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mTabSelectionEditorLayout, TabSelectionEditorLayoutBinder::bind);

        mTabSelectionEditorMediator = new TabSelectionEditorMediator(
                mContext, mTabModelSelector, this::resetWithListOfTabs, mModel, mSelectionDelegate);
    }

    /**
     * @return The {@link SelectionDelegate} that is used in this component.
     */
    SelectionDelegate<Integer> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    /**
     * Gets the view type for each item in the list.
     */
    int getItemViewType(PropertyModel item) {
        return TabGridViewHolder.TabGridViewItemType.SELECTABLE_TAB;
    }

    /**
     * Resets {@link TabListCoordinator} with the provided list.
     * @param tabs List of {@link Tab}s to reset.
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * @return {@link TabSelectionEditorController} that can control the TabSelectionEditor.
     */
    TabSelectionEditorController getController() {
        return mTabSelectionEditorMediator;
    }
}
