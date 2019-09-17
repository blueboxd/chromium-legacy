// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;

/**
 * Interface for the Tasks-related Start Surface. The tasks surface displays information related to
 *  task management, such as the tab switcher, most visited tiles, and omnibox. Implemented by
 *  {@link TasksSurfaceCoordinator}.
 */
public interface TasksSurface {
    /**
     * Set the listener to get the {@link Layout#onTabSelecting} event from the Grid Tab Switcher.
     * @param listener The {@link TabSwitcher.OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener);

    /**
     * @return Controller implementation for overview observation and visibility changes.
     */
    TabSwitcher.Controller getController();

    /**
     * @return TabListDelegate implementation to access the tab grid.
     */
    TabSwitcher.TabListDelegate getTabListDelegate();

    /**
     * Get the container {@link ViewGroup} of the surface.
     * @return The surface's container {@link ViewGroup}.
     */
    ViewGroup getContainerView();

    /**
     * Perform any necessary cleanup.
     */
    void destroy();
}
