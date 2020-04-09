// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

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
}
