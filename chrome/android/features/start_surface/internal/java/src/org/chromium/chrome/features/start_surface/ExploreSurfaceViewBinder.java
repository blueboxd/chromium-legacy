// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the explore surface. */
class ExploreSurfaceViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (propertyKey == IS_EXPLORE_SURFACE_VISIBLE) {
            setVisibility(view, model, model.get(IS_EXPLORE_SURFACE_VISIBLE));
        } else if (propertyKey == IS_SHOWING_OVERVIEW) {
            if (!model.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

            if (model.get(IS_SHOWING_OVERVIEW)) {
                // Set the initial state if the explore surface is selected previously.
                setVisibility(view, model, true);
            } else {
                setVisibility(view, model, false);
            }
        }
    }

    private static void setVisibility(
            ViewGroup containerView, PropertyModel model, boolean isShowing) {
        if (model.get(FEED_SURFACE_COORDINATOR) == null) return;

        View feedSurfaceView = model.get(FEED_SURFACE_COORDINATOR).getView();
        if (isShowing) {
            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
            layoutParams.topMargin = model.get(TOP_BAR_HEIGHT);
            containerView.addView(feedSurfaceView, layoutParams);
        } else {
            UiUtils.removeViewFromParent(feedSurfaceView);
        }
    }
}