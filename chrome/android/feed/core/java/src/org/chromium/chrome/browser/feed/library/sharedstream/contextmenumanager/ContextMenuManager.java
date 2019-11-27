// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager;

import android.view.View;

import java.util.List;

/** Allow for display of context menus. */
public interface ContextMenuManager {
    /**
     * Opens a context menu if there is currently no open context menu. Returns whether a menu was
     * opened.
     *
     * @param anchorView The {@link View} to position the menu by.
     * @param items The contents to display.
     * @param handler The {@link ContextMenuClickHandler} that handles the user clicking on an
     *         option.
     */
    boolean openContextMenu(View anchorView, List<String> items, ContextMenuClickHandler handler);

    /** Dismiss a popup if one is showing. */
    void dismissPopup();

    /**
     * Sets the root view of the window that the context menu is opening in. This, as well as the
     * anchor view that the click happens on, determines where the context menu opens if the context
     * menu is anchored.
     *
     * <p>Note: this is being changed to be settable after the fact for a future version of this
     * library that will use dependency injection.
     */
    void setView(View view);

    /** Notifies creator of the menu that a click has occurred. */
    interface ContextMenuClickHandler {
        void handleClick(int position);
    }
}
