// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Simple TabModelSelector that assumes that only the regular TabModel type exists.
 */
public class SingleTabModelSelector extends TabModelSelectorBase {
    private final SingleTabModel mTabModel;

    public SingleTabModelSelector(Activity activity, TabCreatorManager tabCreatorManager,
            boolean incognito, boolean blockNewWindows) {
        super(tabCreatorManager);
        mTabModel = new SingleTabModel(activity, incognito, blockNewWindows);
        initialize(false, mTabModel);
    }

    public void setTab(Tab tab) {
        mTabModel.setTab(tab);
        markTabStateInitialized();
    }

    @Override
    public void selectModel(boolean incognito) {
        assert incognito == mTabModel.isIncognito();
    }

    @Override
    public TabModel getModel(boolean incognito) {
        return super.getModel(incognito);
    }

    @Override
    public TabModel getCurrentModel() {
        assert super.getCurrentModel() == mTabModel;
        return mTabModel;
    }

    @Override
    public boolean isIncognitoSelected() {
        return mTabModel.isIncognito();
    }

    @Override
    public void closeAllTabs() {
        mTabModel.closeAllTabs();
    }

    @Override
    public int getTotalTabCount() {
        assert mTabModel != null;
        return mTabModel.getCount();
    }

    @Override
    public Tab getTabById(int id) {
        Tab currentTab = getCurrentTab();
        if (currentTab != null && currentTab.getId() == id) return currentTab;
        return null;
    }

    @Override
    public TabModel getModelAt(int index) {
        assert index == INCOGNITO_TAB_MODEL_INDEX || index == NORMAL_TAB_MODEL_INDEX;
        return mTabModel;
    }
}
