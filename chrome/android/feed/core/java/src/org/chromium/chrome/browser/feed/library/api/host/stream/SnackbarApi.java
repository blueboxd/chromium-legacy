// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

/** The SnackbarApi is used by the Feed to render snackbars. */
public interface SnackbarApi {
    /**
     * Displays a snackbar on the host.
     *
     * @param message Text to display in the snackbar.
     */
    void show(String message);

    /**
     * Displays a snackbar on the host.
     *
     * @param message Text to display in the snackbar.
     * @param message Text to display in the snackbar's action. e.g. "undo"
     * @param callback A Callback for the client to know why and when the snackbar goes away.
     */
    void show(String message, String action, SnackbarCallbackApi callback);
}
