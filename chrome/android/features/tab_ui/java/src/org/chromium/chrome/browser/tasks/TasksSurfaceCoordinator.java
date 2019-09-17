// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.LinearLayout;

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
    private final MostVisitedListCoordinator mMostVisitedList;
    private final TasksSurfaceMediator mMediator;

    public TasksSurfaceCoordinator(
            ChromeActivity activity, boolean isTabCarousel, PropertyModel propertyModel) {
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        if (isTabCarousel) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(
                    activity, mView.getTabSwitcherContainer());
        } else {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    activity, mView.getTabSwitcherContainer());
        }

        mMediator = new TasksSurfaceMediator(
                activity, propertyModel, isTabCarousel, activity.getOverviewModeBehavior());

        // TODO(mattsimmons): Handle incognito/dark theming.
        LinearLayout mvTilesLayout = mView.findViewById(R.id.mv_tiles_layout);
        mMostVisitedList = new MostVisitedListCoordinator(activity, mvTilesLayout);
    }

    /** TasksSurface implementation. */
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
    public ViewGroup getContainerView() {
        return mView;
    }

    @Override
    public void destroy() {
        mMediator.destroy();
    }
}
