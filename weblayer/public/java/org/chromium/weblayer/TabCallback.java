// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of a Tab.
 */
public abstract class TabCallback {
    /**
     * The Uri that should be displayed in the location-bar has updated.
     *
     * @param uri The new user-visible uri.
     */
    public void onVisibleUriChanged(@NonNull Uri uri) {}

    /**
     * Triggered when the render process dies, either due to crash or killed by the system to
     * reclaim memory.
     */
    public void onRenderProcessGone() {}

    /**
     * Triggered when a context menu should be displayed.
     * Added in M82.
     */
    public void showContextMenu(@NonNull ContextMenuParams params) {}

    /**
     * Triggered when a tab's contents have been rendered inactive due to a modal overlay, or active
     * due to the dismissal of a modal overlay (dialog/bubble/popup).
     *
     * @param isTabModalShowing true when a dialog is blocking interaction with the web contents.
     *
     * @since 82
     */
    public void onTabModalStateChanged(boolean isTabModalShowing) {}

    /**
     * Called when the title of this tab changes. Note before the page sets a title, the title may
     * be a portion of the Uri.
     * @param title New title of this tab.
     * @since 83
     */
    public void onTitleUpdated(@NonNull String title) {}
}
