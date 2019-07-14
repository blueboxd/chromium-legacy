// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

/**
 * Handles initialization for the downloads system, i.e. creating in-progress download manager or
 * full download manager depending on whether we are in reduced mode or full browser mode.
 */

public class DownloadStartupUtils {
    /**
     * Initializes the downloads system if not already initialized.
     * @param isFullBrowserStarted Whether full browser process has been started.
     */
    public static void ensureDownloadSystemInitialized(boolean isFullBrowserStarted) {
        nativeEnsureDownloadSystemInitialized(isFullBrowserStarted);
    }

    private static native void nativeEnsureDownloadSystemInitialized(boolean isFullBrowserStarted);
}
