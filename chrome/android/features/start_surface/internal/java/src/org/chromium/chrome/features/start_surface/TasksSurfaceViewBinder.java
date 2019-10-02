// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.v4.widget.NestedScrollView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The binder controls the display of the {@link TasksView} in its parent. */
class TasksSurfaceViewBinder {
    /**
     * The view holder holds the parent view, the scroll container and the tasks surface view.
     */
    public static class ViewHolder {
        public final ViewGroup parentView;
        @Nullable
        public final NestedScrollView scrollContainer;
        public final ViewGroup tasksSurfaceView;

        ViewHolder(ViewGroup parentView, @Nullable NestedScrollView scrollContainer,
                ViewGroup tasksSurfaceView) {
            this.parentView = parentView;
            this.scrollContainer = scrollContainer;
            this.tasksSurfaceView = tasksSurfaceView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_SHOWING_OVERVIEW == propertyKey) {
            updateLayoutAndVisibility(viewHolder, model, model.get(IS_SHOWING_OVERVIEW));
        } else if (BOTTOM_BAR_HEIGHT == propertyKey) {
            setBottomBarHeight(viewHolder, model.get(BOTTOM_BAR_HEIGHT));
        }
    }

    private static void updateLayoutAndVisibility(
            ViewHolder viewHolder, PropertyModel model, boolean isShowing) {
        ViewGroup targetView = viewHolder.scrollContainer == null ? viewHolder.tasksSurfaceView
                                                                  : viewHolder.scrollContainer;
        if (isShowing && viewHolder.tasksSurfaceView.getParent() == null) {
            viewHolder.parentView.addView(targetView);
            if (viewHolder.scrollContainer != null) targetView.addView(viewHolder.tasksSurfaceView);
            MarginLayoutParams layoutParams = (MarginLayoutParams) targetView.getLayoutParams();
            layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
            layoutParams.topMargin = model.get(TOP_BAR_HEIGHT);
        }

        targetView.setVisibility(isShowing ? View.VISIBLE : View.GONE);
    }

    private static void setBottomBarHeight(ViewHolder viewHolder, int height) {
        ViewGroup targetView = viewHolder.scrollContainer == null ? viewHolder.tasksSurfaceView
                                                                  : viewHolder.scrollContainer;
        ((MarginLayoutParams) targetView.getLayoutParams()).bottomMargin = height;
    }
}
