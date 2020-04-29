// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
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
    private final TasksSurfaceMediator mMediator;
    private MostVisitedListCoordinator mMostVisitedList;
    private final PropertyModel mPropertyModel;

    public TasksSurfaceCoordinator(ChromeActivity activity, PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, boolean hasMVTiles) {
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mView.initialize(activity.getLifecycleDispatcher());
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        mPropertyModel = propertyModel;
        if (tabSwitcherType == TabSwitcherType.CAROUSEL) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(
                    activity, mView.getCarouselTabSwitcherContainer());
        } else if (tabSwitcherType == TabSwitcherType.GRID) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    activity, mView.getBodyViewContainer());
        } else if (tabSwitcherType == TabSwitcherType.SINGLE) {
            mTabSwitcher = new SingleTabSwitcherCoordinator(
                    activity, mView.getCarouselTabSwitcherContainer());
        } else {
            mTabSwitcher = null;
            assert false : "Unsupported tab switcher type";
        }

        View.OnClickListener incognitoLearnMoreClickListener = v -> {
            HelpAndFeedback.getInstance().show(activity,
                    activity.getString(R.string.help_context_incognito_learn_more),
                    Profile.getLastUsedRegularProfile().getOffTheRecordProfile(), null);
        };
        IncognitoCookieControlsManager incognitoCookieControlsManager =
                new IncognitoCookieControlsManager();
        mMediator = new TasksSurfaceMediator(propertyModel, incognitoLearnMoreClickListener,
                incognitoCookieControlsManager, tabSwitcherType == TabSwitcherType.CAROUSEL);

        if (hasMVTiles) {
            LinearLayout mvTilesLayout = mView.findViewById(R.id.mv_tiles_layout);
            mMostVisitedList =
                    new MostVisitedListCoordinator(activity, mvTilesLayout, mPropertyModel);
        }
    }

    /** TasksSurface implementation. */
    @Override
    public void initialize() {
        assert LibraryLoader.getInstance().isInitialized();

        if (mMostVisitedList != null) mMostVisitedList.initialize();
        mMediator.initialize();
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
    public ViewGroup getBodyViewContainer() {
        return mView.getBodyViewContainer();
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void onFinishNativeInitialization(Context context, FakeboxDelegate fakeboxDelegate) {
        ChromeActivity activity = (ChromeActivity) context;
        mTabSwitcher.initWithNative(activity, activity.getTabContentManager(),
                activity.getCompositorViewHolder().getDynamicResourceLoader(), activity,
                activity.getModalDialogManager());

        mMediator.initWithNative(fakeboxDelegate);
    }
}
