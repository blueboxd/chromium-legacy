// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.v4.widget.NestedScrollView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SurfaceMode;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final ChromeActivity mActivity;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final @SurfaceMode int mSurfaceMode;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private PropertyModelChangeProcessor mTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModelChangeProcessor mSecondaryTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.NO_START_SURFACE to show the tabs.
    @Nullable
    private TabSwitcher mTabSwitcher;

    // Non-null in SurfaceMode.TWO_PANES mode.
    @Nullable
    private BottomBarCoordinator mBottomBarCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable
    private PropertyModel mPropertyModel;

    // Used to remember TabSwitcher.OnTabSelectingListener in SurfaceMode.SINGLE_PANE mode for more
    // tabs surface if necessary.
    @Nullable
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mSurfaceMode = computeSurfaceMode();

        if (mSurfaceMode == SurfaceMode.NO_START_SURFACE) {
            // Create Tab switcher directly to save one layer in the view hierarchy.
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    mActivity, mActivity.getCompositorViewHolder());
        } else {
            createAndSetStartSurface();
        }

        TabSwitcher.Controller controller =
                mTabSwitcher != null ? mTabSwitcher.getController() : mTasksSurface.getController();
        mStartSurfaceMediator = new StartSurfaceMediator(controller,
                mActivity.getTabModelSelector(), mPropertyModel,
                mExploreSurfaceCoordinator == null
                        ? null
                        : mExploreSurfaceCoordinator.getFeedSurfaceCreator(),
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? this::initializeSecondaryTasksSurface
                                                        : null,
                mSurfaceMode);
    }

    // Implements StartSurface.
    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        if (mTasksSurface != null) {
            mTasksSurface.setOnTabSelectingListener(listener);
        } else {
            mTabSwitcher.setOnTabSelectingListener(listener);
        }

        // Set OnTabSelectingListener to the more tabs tasks surface as well if it has been
        // instantiated, otherwise remember it for the future instantiation.
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            if (mSecondaryTasksSurface == null) {
                mOnTabSelectingListener = listener;
            } else {
                mSecondaryTasksSurface.setOnTabSelectingListener(listener);
            }
        }
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        if (mTasksSurface != null) {
            return mTasksSurface.getTabListDelegate();
        }

        return mTabSwitcher.getTabListDelegate();
    }

    private @SurfaceMode int computeSurfaceMode() {
        String feature = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation");

        // Do not enable two panes when the bottom bar is enabled since it will
        // overlap the two panes' bottom bar.
        if (feature.equals("twopanes") && !FeatureUtilities.isBottomToolbarEnabled()) {
            return SurfaceMode.TWO_PANES;
        }

        if (feature.equals("single")) return SurfaceMode.SINGLE_PANE;

        if (feature.equals("tasksonly")) return SurfaceMode.TASKS_ONLY;

        return SurfaceMode.NO_START_SURFACE;
    }

    private void createAndSetStartSurface() {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);
        mPropertyModel.set(TOP_BAR_HEIGHT,
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow));

        mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, mSurfaceMode == SurfaceMode.SINGLE_PANE, mPropertyModel);

        // The tasks surface is added to the explore surface in the single pane mode below.
        if (mSurfaceMode != SurfaceMode.SINGLE_PANE) {
            // TODO(crbug.com/1000295): Make the tasks surface view scrollable together with the Tab
            // switcher view in tasks only mode.
            mTasksSurfacePropertyModelChangeProcessor =
                    createTasksViewPropertyModelChangeProcessor(mTasksSurface.getContainerView(),
                            TasksSurfaceViewBinder::bind, mSurfaceMode != SurfaceMode.TASKS_ONLY);
        }

        // There is nothing else to do for SurfaceMode.TASKS_ONLY.
        if (mSurfaceMode == SurfaceMode.TASKS_ONLY) {
            return;
        }

        if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            mBottomBarCoordinator = new BottomBarCoordinator(
                    mActivity, mActivity.getCompositorViewHolder(), mPropertyModel);
        }

        // TODO(crbug.com/1000295): Hide Tab switcher toolbar on explore pane in two panes mode,
        // which should be done when adding fake search box.
        mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator(mActivity,
                mActivity.getCompositorViewHolder(),
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? mTasksSurface.getContainerView() : null,
                mPropertyModel);
    }

    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;
        assert mSecondaryTasksSurface == null;

        PropertyModel propertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mStartSurfaceMediator.setSecondaryTasksSurfacePropertyModel(propertyModel);
        mSecondaryTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, false, propertyModel);
        mSecondaryTasksSurfacePropertyModelChangeProcessor =
                createTasksViewPropertyModelChangeProcessor(
                        mSecondaryTasksSurface.getContainerView(),
                        SecondaryTasksSurfaceViewBinder::bind, true);
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }
        return mSecondaryTasksSurface.getController();
    }

    private PropertyModelChangeProcessor createTasksViewPropertyModelChangeProcessor(
            ViewGroup container,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel,
                    TasksSurfaceViewBinder.ViewHolder, PropertyKey> binder,
            boolean needScrollContainer) {
        // TODO(crbug.com/1000295): Put TasksSurface view inside Tab switcher recycler view.
        // This is a temporarily solution to make the TasksSurfaceView scroll together with the
        // Tab switcher, however this solution has performance issue when the Tab switcher is in
        // Grid mode, which is a launcher blocker. Check the bug details.
        return PropertyModelChangeProcessor.create(mPropertyModel,
                new TasksSurfaceViewBinder.ViewHolder(mActivity.getCompositorViewHolder(),
                        needScrollContainer
                                ? (NestedScrollView) LayoutInflater.from(mActivity).inflate(
                                        R.layout.ss_explore_scroll_container,
                                        mActivity.getCompositorViewHolder(), false)
                                : null,
                        container),
                binder);
    }
}
