// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is the mediator that contains all business logic for TabSelectionEditor component. It
 * is also responsible for resetting the selectable tab grid based on visibility property.
 */
class TabSelectionEditorMediator
        implements TabSelectionEditorCoordinator.TabSelectionEditorController {
    // TODO(977271): Unify similar interfaces in other components that used the TabListCoordinator.
    /**
     * Interface for resetting the selectable tab grid.
     */
    interface ResetHandler {
        /**
         * Handles the reset event.
         * @param tabs List of {@link Tab}s to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final ResetHandler mResetHandler;
    private final PropertyModel mModel;
    private final SelectionDelegate<Integer> mSelectionDelegate;

    private final View.OnClickListener mNavigationClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            RecordUserAction.record("TabMultiSelect.Cancelled");
            hide();
        }
    };

    private final View.OnClickListener mGroupButtonOnClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            List<Tab> selectedTabs = new ArrayList<>();

            for (int tabId : mSelectionDelegate.getSelectedItems()) {
                selectedTabs.add(
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
            }

            Tab destinationTab = getDestinationTab(selectedTabs);

            TabGroupModelFilter tabGroupModelFilter =
                    (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter();

            tabGroupModelFilter.mergeListOfTabsToGroup(selectedTabs, destinationTab);

            hide();

            RecordUserAction.record("TabMultiSelect.Done");
            RecordUserAction.record("TabGroup.Created.TabMultiSelect");
        }
    };

    TabSelectionEditorMediator(Context context, TabModelSelector tabModelSelector,
            ResetHandler resetHandler, PropertyModel model,
            SelectionDelegate<Integer> selectionDelegate) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mResetHandler = resetHandler;
        mModel = model;
        mSelectionDelegate = selectionDelegate;

        mModel.set(
                TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_GROUP_BUTTON_LISTENER,
                mGroupButtonOnClickListener);
    }

    /**
     * @return The {@link Tab} that has the greatest index in TabModel among the given list of tabs.
     */
    private Tab getDestinationTab(List<Tab> tabs) {
        int greatestIndex = TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < tabs.size(); i++) {
            int index = TabModelUtils.getTabIndexById(
                    mTabModelSelector.getCurrentModel(), tabs.get(i).getId());
            greatestIndex = Math.max(index, greatestIndex);
        }
        return mTabModelSelector.getCurrentModel().getTabAt(greatestIndex);
    }

    private void hide() {
        mResetHandler.resetWithListOfTabs(null);
        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, false);
    }

    private boolean isEditorVisible() {
        return mModel.get(TabSelectionEditorProperties.IS_VISIBLE);
    }

    @Override
    public void show() {
        mResetHandler.resetWithListOfTabs(getTabsToShow());
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);
        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, true);
    }

    private List<Tab> getTabsToShow() {
        List<Tab> tabs = new ArrayList<>();
        TabModelFilter tabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        for (int i = 0; i < tabModelFilter.getCount(); i++) {
            Tab tab = tabModelFilter.getTabAt(i);
            // TODO(977302): This filtered out the tabs that has related tabs. This should be done
            // at the TabModelFilter.
            if (tabModelFilter.getRelatedTabList(tab.getId()).size() == 1) {
                tabs.add(tab);
            }
        }
        return tabs;
    }

    @Override
    public boolean handleBackPressed() {
        if (!isEditorVisible()) return false;
        hide();
        RecordUserAction.record("TabMultiSelect.Cancelled");
        return true;
    }
}
