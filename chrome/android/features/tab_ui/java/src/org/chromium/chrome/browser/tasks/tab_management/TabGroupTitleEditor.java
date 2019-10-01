// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class hosts logic related to edit tab group title. Concrete class that extends this abstract
 * class needs to specify the title storage/fetching implementation as well as handle {@link
 * PropertyModel} update.
 */
public abstract class TabGroupTitleEditor {
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilter.Observer mFilterObserver;

    public TabGroupTitleEditor(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                int tabRootId = tab.getRootId();
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                // If the group becomes a single tab after closing or we are closing a group, delete
                // the stored title.
                if (filter.getRelatedTabListForRootId(tabRootId).size() == 1) {
                    deleteTabGroupTitle(tabRootId);
                }
            }
        };

        mFilterObserver = new EmptyTabGroupModelFilterObserver() {
            @Override
            public void willMergeTabToGroup(Tab movedTab) {
                deleteTabGroupTitle(movedTab.getRootId());
            }

            @Override
            public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                String title = getTabGroupTitle(movedTab.getRootId());
                if (title == null) return;
                // If the group size is 2, i.e. the group becomes a single tab after ungroup, delete
                // the stored title.
                if (filter.getRelatedTabList(movedTab.getId()).size() == 2) {
                    deleteTabGroupTitle(movedTab.getRootId());
                    return;
                }
                // If the root tab in group is moved out, try to fetch the title and
                // reassign it to the new root tab in group.
                if (movedTab.getRootId() != newRootId) {
                    updateTabGroupTitle(mTabModelSelector.getTabById(newRootId), title);
                }
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false)
                        instanceof TabGroupModelFilter;
        assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true)
                        instanceof TabGroupModelFilter;
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 false))
                .addTabGroupObserver(mFilterObserver);
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 true))
                .addTabGroupObserver(mFilterObserver);
    }

    /**
     * This method uses tab group root ID of {@code tab} as a reference to update tab group title.
     * Implementation of this method needs to store the updated title and maybe update relevant
     * {@link PropertyModel} as well.
     *
     * @param tab     The {@link Tab} whose root ID is used as reference to update group title.
     * @param title   The tab group title to store.
     */
    protected abstract void updateTabGroupTitle(Tab tab, String title);

    /**
     * This method deletes specific stored tab group title.
     * @param tabRootId   The tab root ID whose related tab group title will be deleted.
     */
    protected abstract void deleteTabGroupTitle(int tabRootId);

    /**
     * This method fetches tab group title with related tab group root ID.
     * @param tabRootId  The tab root ID whose related tab group title will be deleted.
     * @return The stored title of the related group, default value is null.
     */
    protected abstract String getTabGroupTitle(int tabRootId);

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 false))
                .removeTabGroupObserver(mFilterObserver);
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 true))
                .removeTabGroupObserver(mFilterObserver);
    }
}
