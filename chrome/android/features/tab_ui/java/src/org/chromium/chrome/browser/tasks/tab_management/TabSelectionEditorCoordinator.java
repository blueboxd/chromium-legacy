// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
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
         * Hides the TabSelectionEditor.
         */
        void hide();

        /**
         * @return Whether the TabSelectionEditor is visible.
         */
        boolean isEditorVisible();
    }

    private final Context mContext;
    private final View mParentView;
    private final TabModelSelector mTabModelSelector;

    public TabSelectionEditorCoordinator(Context context, View parentView,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager) {
        mContext = context;
        mParentView = parentView;
        mTabModelSelector = tabModelSelector;
    }

    private void resetWithListOfTabs(@Nullable List<Tab> tab) {
        // TODO(meiliang): reset TabListCoordinator and TabSelectionEditorMediator
    }

    TabSelectionEditorController getController() {
        // TODO(meiliang): move this to TabSelectionEditorMediator.
        return new TabSelectionEditorController() {
            @Override
            public void show() {
                List<Tab> tabs = new ArrayList<>();
                TabModelFilter tabModelFilter =
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
                for (int i = 0; i < tabModelFilter.getCount(); i++) {
                    Tab tab = tabModelFilter.getTabAt(i);
                    if (tabModelFilter.getRelatedTabList(tab.getId()).size() == 1) {
                        tabs.add(tab);
                    }
                }
                resetWithListOfTabs(tabs);
            }

            @Override
            public void hide() {
                resetWithListOfTabs(null);
            }

            @Override
            public boolean isEditorVisible() {
                // TODO(meiliang): get visibility from PropertyModel
                return false;
            }
        };
    }
}
