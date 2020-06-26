// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

/**
 * Interface to provide chromium calling points for an external surface.
 */
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    /**
     * Navigates the current tab to a particular URL.
     */
    default void navigateTab(String url) {}

    /**
     * Navigates a new tab to a particular URL.
     */
    default void navigateNewTab(String url) {}

    /**
     * Navigate a new incognito tab to a URL.
     */
    default void navigateIncognitoTab(String url) {}

    /**
     * Get an offline page for a URL.
     */
    default void downloadLink(String url) {}

    /**
     * Open a bottom sheet with the view as contents.
     */
    default void showBottomSheet(View view) {}

    /**
     * Dismiss the open bottom sheet (or do nothing if there isn't one).
     */
    default void dismissBottomSheet() {}
}
