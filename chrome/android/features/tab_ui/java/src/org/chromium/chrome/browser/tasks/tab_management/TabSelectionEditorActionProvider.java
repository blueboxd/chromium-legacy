// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Provider of actions for a list of selected tabs in {@link TabSelectionEditorMediator}.
 */
class TabSelectionEditorActionProvider {
    @IntDef({TabSelectionEditorAction.UNGROUP, TabSelectionEditorAction.GROUP})
    @Retention(RetentionPolicy.SOURCE)
    @interface TabSelectionEditorAction {
        int GROUP = 0;
        int UNGROUP = 1;
        int NUM_ENTRIES = 2;
    }
    private final TabModelSelector mTabModelSelector;
    private final TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private final int mAction;

    TabSelectionEditorActionProvider(TabModelSelector tabModelSelector,
            TabSelectionEditorCoordinator.TabSelectionEditorController tabSelectionEditorController,
            @TabSelectionEditorAction int action) {
        mTabModelSelector = tabModelSelector;
        mTabSelectionEditorController = tabSelectionEditorController;
        mAction = action;
    }

    /**
     * Defines how to process {@code selectedTabs} based on the {@link TabSelectionEditorAction}
     * specified in the constructor.
     *
     * @param selectedTabs The list of selected tabs to process.
     */
    void processSelectedTabs(List<Tab> selectedTabs) {
        assert mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;

        switch (mAction) {
            case TabSelectionEditorAction.GROUP:
                Tab destinationTab = getDestinationTab(selectedTabs);

                TabGroupModelFilter tabGroupModelFilter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                tabGroupModelFilter.mergeListOfTabsToGroup(
                        selectedTabs, destinationTab, false, true);
                mTabSelectionEditorController.hide();

                RecordUserAction.record("TabMultiSelect.Done");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabSelectionEditorAction.UNGROUP:
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                for (Tab tab : selectedTabs) {
                    filter.moveTabOutOfGroup(tab.getId());
                }
                mTabSelectionEditorController.hide();
                break;
            default:
                assert false;
        }
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
}
