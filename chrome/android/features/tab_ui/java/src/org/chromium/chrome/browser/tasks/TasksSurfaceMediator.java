// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TOP_PADDING;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for handling {@link TasksSurface}-related logic.
 */
class TasksSurfaceMediator implements OverviewModeBehavior.OverviewModeObserver {
    private final PropertyModel mModel;

    private OverviewModeBehavior mOverviewModeBehavior;

    TasksSurfaceMediator(Context context, PropertyModel model, boolean isTabCarousel,
            OverviewModeBehavior overviewModeBehavior) {
        mModel = model;

        mModel.set(IS_TAB_CAROUSEL, isTabCarousel);

        if (!isTabCarousel) {
            mModel.set(TOP_PADDING,
                    context.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                            * 2);
        }

        // TODO(mattsimmons): Move all of the tasks surface visibility changes here and handle it
        //  for all surface variations. This is not ideal as-is.
        if (ReturnToChromeExperimentsUtil.shouldShowMostVisitedOnTabSwitcher()) {
            mOverviewModeBehavior = overviewModeBehavior;
            mOverviewModeBehavior.addOverviewModeObserver(this);
        }
    }

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        mModel.set(MV_TILES_VISIBLE, true);
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        mModel.set(MV_TILES_VISIBLE, false);
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    void destroy() {
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(this);
            mOverviewModeBehavior = null;
        }
    }
}
