// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.v4.widget.NestedScrollView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.Nullable;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the explore surface. */
class ExploreSurfaceViewBinder {
    /**
     * The view holder holds the parent view, the possible scroll container and header container
     * view.
     */
    public static class ViewHolder {
        public final ViewGroup parentView;
        @Nullable
        public final NestedScrollView scrollContainer;
        @Nullable
        public final ViewGroup headerContainerView;

        ViewHolder(ViewGroup parentView, @Nullable NestedScrollView scrollContainer,
                @Nullable ViewGroup headerContainerView) {
            assert (scrollContainer == null) == (headerContainerView == null);
            this.parentView = parentView;
            this.scrollContainer = scrollContainer;
            this.headerContainerView = headerContainerView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == IS_EXPLORE_SURFACE_VISIBLE) {
            setVisibility(view, model,
                    model.get(IS_EXPLORE_SURFACE_VISIBLE) && model.get(IS_SHOWING_OVERVIEW));
        } else if (propertyKey == IS_SHOWING_OVERVIEW) {
            setVisibility(view, model,
                    model.get(IS_EXPLORE_SURFACE_VISIBLE) && model.get(IS_SHOWING_OVERVIEW));
        }
    }

    /**
     * Set the explore surface visibility.
     * Note that if the {@link ViewHolder.headerContainerView} is not null, the feed surface view is
     * added to the {@link ViewHolder.headerContainerView}. This is for the alignment between the
     * {@link ViewHolder.headerContainerView} and the feed surface view to avoid another level of
     * view hiearachy. Then {@link ViewHolder.headerContainerView} is added to {@link
     * ViewHolder.scrollContainer}. This allows the {@link ViewHolder.headerContainerView} scrolls
     * together with the feed surface view. Finally, we add the {@link ViewHolder.scrollContainer}
     * to the {@link ViewHolder.parentView}. If the {@link ViewHolder.headerContainerView} is null,
     * then the feed surface view is added to the {@link ViewHolder.parentView} directly.
     * @param viewHolder The view holder holds the parent and possible the header container view.
     * @param model The property model.
     * @param isShowing Whether set the surface to visible or not.
     */
    private static void setVisibility(
            ViewHolder viewHolder, PropertyModel model, boolean isShowing) {
        if (model.get(FEED_SURFACE_COORDINATOR) == null) return;

        if (viewHolder.headerContainerView != null) {
            if (viewHolder.scrollContainer.getParent() == null) {
                viewHolder.scrollContainer.addView(viewHolder.headerContainerView);
                FrameLayout.LayoutParams layoutParams =
                        (FrameLayout.LayoutParams) viewHolder.scrollContainer.getLayoutParams();
                layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
                layoutParams.topMargin = model.get(TOP_BAR_HEIGHT);
                viewHolder.parentView.addView(viewHolder.scrollContainer);
            }
            viewHolder.scrollContainer.setVisibility(isShowing ? View.VISIBLE : View.GONE);
        }

        View feedSurfaceView = model.get(FEED_SURFACE_COORDINATOR).getView();
        if (isShowing) {
            if (viewHolder.headerContainerView == null) {
                FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
                layoutParams.topMargin = model.get(TOP_BAR_HEIGHT);
                viewHolder.parentView.addView(feedSurfaceView, layoutParams);
            } else {
                viewHolder.headerContainerView.addView(feedSurfaceView);
            }
        } else {
            UiUtils.removeViewFromParent(feedSurfaceView);
        }
    }
}
