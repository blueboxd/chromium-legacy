// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

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
         * Shows the TabSelectionEditor with the given {@link Tab}s.
         * @param tabs List of {@link Tab}s to show.
         */
        void show(List<Tab> tabs);

        /**
         * Hides the TabSelectionEditor.
         */
        void hide();

        /**
         * @return Whether or not the TabSelectionEditor consumed the event.
         */
        boolean handleBackPressed();

        /**
         * Configure the Toolbar for TabSelectionEditor. The default button text is "Group".
         * @param actionButtonText Button text for the action button.
         * @param actionProvider The {@link TabSelectionEditorActionProvider} that specifies the
         *         action when action button gets clicked.
         * @param actionButtonEnablingThreshold The minimum threshold to enable the action button.
         *         If it's -1 use the default value.
         * @param navigationButtonOnClickListener Click listener for the navigation button.
         */
        void configureToolbar(@Nullable String actionButtonText,
                @Nullable TabSelectionEditorActionProvider actionProvider,
                int actionButtonEnablingThreshold,
                @Nullable View.OnClickListener navigationButtonOnClickListener);
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
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            TabSelectionEditorLayout.TabSelectionEditorLayoutPositionProvider positionProvider) {
        mContext = context;
        mParentView = parentView;
        mTabModelSelector = tabModelSelector;
        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                mTabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false,
                null, null, null, TabProperties.UiType.SELECTABLE, this::getSelectionDelegate, null,
                null, false, COMPONENT_NAME);

        mTabSelectionEditorLayout = LayoutInflater.from(context)
                .inflate(R.layout.tab_selection_editor_layout, null)
                .findViewById(R.id.selectable_list);
        mTabSelectionEditorLayout.initialize(mParentView, mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(), mSelectionDelegate,
                positionProvider);
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

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mTabSelectionEditorLayout.destroy();
        mTabSelectionEditorMediator.destroy();
        mTabSelectionEditorLayoutChangeProcessor.destroy();
    }
}
