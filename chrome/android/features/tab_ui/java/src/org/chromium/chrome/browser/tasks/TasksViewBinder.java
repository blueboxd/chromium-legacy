// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TOP_PADDING;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the tasks surface view.
class TasksViewBinder {
    public static void bind(PropertyModel model, TasksView view, PropertyKey propertyKey) {
        if (propertyKey == IS_TAB_CAROUSEL) {
            view.setIsTabCarousel(model.get(IS_TAB_CAROUSEL));
        } else if (propertyKey == MORE_TABS_CLICK_LISTENER) {
            view.setMoreTabsOnClickListener(model.get(MORE_TABS_CLICK_LISTENER));
        } else if (propertyKey == MV_TILES_VISIBLE) {
            // TODO(mattsimmons): Hide these when in incognito mode.
            view.setMostVisitedVisibility(model.get(MV_TILES_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TOP_PADDING) {
            view.setPadding(0, model.get(TOP_PADDING), 0, 0);
        }
    }
}