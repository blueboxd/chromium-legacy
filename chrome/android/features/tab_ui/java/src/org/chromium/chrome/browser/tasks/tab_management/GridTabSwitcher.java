// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.support.annotation.NonNull;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Interface for the Grid Tab Switcher.
 */
public interface GridTabSwitcher {
    // TODO(960196): Remove the following interfaces when the associated bug is resolved.

    /**
     * Interface to control the GridTabSwitcher.
     */
    interface GridController extends OverviewModeController {
        /**
         * Called by the GridTabSwitcherLayout when the system back button is pressed.
         * @return Whether or not the GridTabSwitcher consumed the event.
         */
        boolean onBackPressed();
    }

    /**
     * An observer that is notified when the GridTabSwitcher view state changes.
     */
    interface GridVisibilityObserver extends OverviewModeBehavior.OverviewModeObserver {}

    /**
     * @return GridController implementation that can be used for controlling
     *         grid visibility changes.
     */
    GridController getGridController();

    /**
     * @return The dynamic resource ID of the GridTabSwitcher RecyclerView.
     */
    int getResourceId();

    /**
     * @return The timestamp of last dirty event of {@link ViewResourceAdapter} of
     * {@link TabListRecyclerView}.
     */
    long getLastDirtyTimeForTesting();

    /**
     * Before calling {@link OverviewModeController#showOverview} to start showing the
     * GridTabSwitcher {@link TabListRecyclerView}, call this to populate it without making it
     * visible.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    boolean prepareOverview();

    /**
     * This is called after the compositor animation is done, for potential clean-up work.
     * {@link OverviewModeBehavior.OverviewModeObserver#onOverviewModeFinishedHiding} happens after
     * the Android View animation, but before the compositor animation.
     */
    void postHiding();

    /**
     * @param forceUpdate Whether to measure the current location again. If not, return the last
     *                    location measured on last layout, which can be wrong after scrolling.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         GridTabSwitcher {@link TabListRecyclerView} coordinates.
     */
    @NonNull
    Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate);
}
