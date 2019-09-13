// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

/**
 * A factory for creating an {@link AppMenuCoordinator}.
 */
public class AppMenuCoordinatorFactory {
    private AppMenuCoordinatorFactory() {}

    /**
     * Create a new AppMenuCoordinator.
     * @param context The activity context.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *         activity.
     * @param buttonDelegate The {@link ToolbarManager} for the containing activity.
     * @param appMenuDelegate The {@link AppMenuDelegate} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *         activity.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     */
    public static AppMenuCoordinator createAppMenuCoordinator(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate, AppMenuDelegate appMenuDelegate, View decorView,
            @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        return new AppMenuCoordinatorImpl(context, activityLifecycleDispatcher, buttonDelegate,
                appMenuDelegate, decorView, overviewModeBehaviorSupplier);
    }
}
