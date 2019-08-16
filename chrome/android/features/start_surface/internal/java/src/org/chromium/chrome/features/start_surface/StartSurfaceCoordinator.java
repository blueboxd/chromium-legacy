// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.view.ViewGroup;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    @IntDef({SurfaceMode.NO_START_SURFACE, SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES,
            SurfaceMode.SINGLE_PANE})
    @Retention(RetentionPolicy.SOURCE)
    @interface SurfaceMode {
        int NO_START_SURFACE = 0;
        int TASKS_ONLY = 1;
        int TWO_PANES = 2;
        int SINGLE_PANE = 3;
    }

    private final ChromeActivity mActivity;
    private final @SurfaceMode int mSurfaceMode;
    private final StartSurfaceMediator mStartSurfaceMediator;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private PropertyModel mTasksSurfacePropertyModel;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModel mSecondaryTasksSurfacePropertyModel;

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
            mSecondaryTasksSurfacePropertyModel =
                    new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
            mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(mActivity,
                    mSurfaceMode == SurfaceMode.SINGLE_PANE, mSecondaryTasksSurfacePropertyModel);
            createAndSetStartSurface();
        }

        StartSurfaceMediator.OverlayVisibilityHandler overlayVisibilityHandler =
                new StartSurfaceMediator.OverlayVisibilityHandler() {
                    @Override
                    // TODO(crbug.com/982018): Consider moving this to LayoutManager.
                    public void setContentOverlayVisibility(boolean isVisible) {
                        if (mActivity.getTabModelSelector().getCurrentTab() == null) return;
                        mActivity.getCompositorViewHolder().setContentOverlayVisibility(
                                isVisible, true);
                    }
                };
        TabSwitcher.Controller controller =
                mTabSwitcher != null ? mTabSwitcher.getController() : mTasksSurface.getController();
        mStartSurfaceMediator = new StartSurfaceMediator(controller,
                mActivity.getTabModelSelector(), overlayVisibilityHandler, mPropertyModel,
                mExploreSurfaceCoordinator == null
                        ? null
                        : mExploreSurfaceCoordinator.getFeedSurfaceCreator(),
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? this::initializeSecondaryTasksSurface
                                                        : null,
                mSurfaceMode == SurfaceMode.SINGLE_PANE);

        // TODO(crbug.com/982018): Consider merging mSecondaryTasksSurfacePropertyModel with
        // mPropertyModel, so mStartSurfaceMediator can set MORE_TABS_CLICK_LISTENER by itself.
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            mSecondaryTasksSurfacePropertyModel.set(
                    MORE_TABS_CLICK_LISTENER, mStartSurfaceMediator);
        }
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

        // TODO(crbug.com/982018): Add the task only surface variation.
        return SurfaceMode.NO_START_SURFACE;
    }

    private void createAndSetStartSurface() {
        assert mTasksSurface != null;

        // The tasks surface is added to the explore surface in the single pane mode below.
        if (mSurfaceMode != SurfaceMode.SINGLE_PANE) {
            mActivity.getCompositorViewHolder().addView(mTasksSurface.getView());
        }

        // There is nothing else to do for SurfaceMode.TASKS_ONLY for now.
        if (mSurfaceMode != SurfaceMode.TWO_PANES && mSurfaceMode != SurfaceMode.SINGLE_PANE) {
            return;
        }

        mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
        if (mSurfaceMode == SurfaceMode.TWO_PANES) createAndSetBottomBar();

        int toolbarHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int topControlsHeight =
                ReturnToChromeExperimentsUtil.shouldShowOmniboxOnTabSwitcher() ? toolbarHeight : 0;

        // Do not hide the top tab switcher toolbar on the single pane start surface for now.
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) topControlsHeight += toolbarHeight;
        mPropertyModel.set(TOP_BAR_HEIGHT, topControlsHeight);

        // Create the explore surface.
        // TODO(crbug.com/982018): This is a hack to hide the top tab switcher toolbar in
        // the explore surface. Remove it after deciding on where to put the omnibox.
        ViewGroup exploreSurfaceContainer =
                (ViewGroup) mActivity.getCompositorViewHolder().getParent();
        mExploreSurfaceCoordinator =
                new ExploreSurfaceCoordinator(mActivity, exploreSurfaceContainer,
                        mSurfaceMode == SurfaceMode.SINGLE_PANE ? mTasksSurface.getView() : null,
                        mPropertyModel);
    }

    private void createAndSetBottomBar() {
        // Margin the bottom of the Tab grid to save space for the bottom bar.
        int bottomBarHeight =
                ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                        R.dimen.ss_bottom_bar_height);
        mTasksSurface.getTabListDelegate().setBottomControlsHeight(bottomBarHeight);
        mPropertyModel.set(BOTTOM_BAR_HEIGHT, bottomBarHeight);

        // Create the bottom bar.
        mBottomBarCoordinator = new BottomBarCoordinator(
                mActivity, mActivity.getCompositorViewHolder(), mPropertyModel);
    }

    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;
        assert mSecondaryTasksSurface == null;

        mSecondaryTasksSurfacePropertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mSecondaryTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(
                mActivity, false, mSecondaryTasksSurfacePropertyModel);
        mActivity.getCompositorViewHolder().addView(mSecondaryTasksSurface.getView());
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }
        return mSecondaryTasksSurface.getController();
    }
}
