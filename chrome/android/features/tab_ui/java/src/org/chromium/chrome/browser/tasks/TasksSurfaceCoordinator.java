// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;

import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying task-related surfaces (Tab Switcher, MV Tiles, Omnibox, etc.).
 *  Concrete implementation of {@link TasksSurface}.
 */
public class TasksSurfaceCoordinator implements TasksSurface {
    private final TabSwitcher mTabSwitcher;
    private final TasksView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public TasksSurfaceCoordinator(
            ChromeActivity activity, boolean isTabCarousel, PropertyModel propertyModel) {
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        if (isTabCarousel) {
            propertyModel.set(IS_TAB_CAROUSEL, true);
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(
                    activity, mView.getTabSwitcherContainer());
        } else {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    activity, mView.getTabSwitcherContainer());
        }
    }

    @Override
    public void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mTabSwitcher.setOnTabSelectingListener(listener);
    }

    @Override
    public TabSwitcher.Controller getController() {
        return mTabSwitcher.getController();
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        return mTabSwitcher.getTabListDelegate();
    }

    @Override
    public View getView() {
        return mView;
    }
}
