// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.TITLE;

import android.graphics.drawable.Drawable;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator of the single tab tab switcher. */
class SingleTabSwitcherMediator implements TabSwitcher.Controller {
    private final ObserverList<TabSwitcher.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabModelObserver mNormalTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private TabSwitcher.OnTabSelectingListener mTabSelectingListener;
    private boolean mShouldIgnoreNextSelect;

    SingleTabSwitcherMediator(PropertyModel propertyModel, TabModelSelector tabModelSelector,
            TabListFaviconProvider tabListFaviconProvider) {
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mTabListFaviconProvider = tabListFaviconProvider;

        mPropertyModel.set(FAVICON, mTabListFaviconProvider.getDefaultFaviconDrawable(false));
        mPropertyModel.set(CLICK_LISTENER, v -> {
            TabModel normalTabModel = mTabModelSelector.getModel(false);
            if (mTabSelectingListener != null
                    && normalTabModel.index() != TabList.INVALID_TAB_INDEX) {
                mTabSelectingListener.onTabSelecting(LayoutManager.time(),
                        normalTabModel.getTabAt(normalTabModel.index()).getId());
            }
        });

        mNormalTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                assert overviewVisible();
                if (mShouldIgnoreNextSelect) {
                    mShouldIgnoreNextSelect = false;
                    return;
                }
                mTabSelectingListener.onTabSelecting(LayoutManager.time(), tab.getId());
            }
        };
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                if (!newModel.isIncognito()) mShouldIgnoreNextSelect = true;
            }
        };
    }

    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mTabSelectingListener = listener;
    }

    // Controller implementation
    @Override
    public boolean overviewVisible() {
        return mPropertyModel.get(IS_VISIBLE);
    }

    @Override
    public void addOverviewModeObserver(TabSwitcher.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(TabSwitcher.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        mShouldIgnoreNextSelect = false;
        mTabModelSelector.getModel(false).removeObserver(mNormalTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);

        mPropertyModel.set(IS_VISIBLE, false);
        mPropertyModel.set(FAVICON, mTabListFaviconProvider.getDefaultFaviconDrawable(false));
        mPropertyModel.set(TITLE, "");

        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    @Override
    public void showOverview(boolean animate) {
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        normalTabModel.addObserver(mNormalTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        int selectedTabIndex = normalTabModel.index();
        if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
            assert normalTabModel.getCount() > 0;

            Tab tab = normalTabModel.getTabAt(selectedTabIndex);
            mPropertyModel.set(TITLE, tab.getTitle());
            mTabListFaviconProvider.getFaviconForUrlAsync(tab.getUrlString(), false,
                    (Drawable favicon) -> { mPropertyModel.set(FAVICON, favicon); });
        }
        mPropertyModel.set(IS_VISIBLE, true);

        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public void enableRecordingFirstMeaningfulPaint(long activityCreateTimeMs) {}
}