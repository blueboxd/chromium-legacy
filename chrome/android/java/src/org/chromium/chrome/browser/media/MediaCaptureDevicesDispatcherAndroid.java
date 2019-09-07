// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.content_public.browser.WebContents;

/**
 * Java access point for MediaCaptureDevicesDispatcher, allowing for querying and manipulation of
 * media capture state.
 */
public class MediaCaptureDevicesDispatcherAndroid {
    public static boolean isCapturingAudio(WebContents webContents) {
        if (webContents == null) return false;
        return nativeIsCapturingAudio(webContents);
    }

    public static boolean isCapturingVideo(WebContents webContents) {
        if (webContents == null) return false;
        return nativeIsCapturingVideo(webContents);
    }

    public static boolean isCapturingScreen(WebContents webContents) {
        if (webContents == null) return false;
        return nativeIsCapturingScreen(webContents);
    }

    public static void notifyStopped(WebContents webContents) {
        if (webContents == null) return;
        nativeNotifyStopped(webContents);
    }

    private static native boolean nativeIsCapturingAudio(WebContents webContents);
    private static native boolean nativeIsCapturingVideo(WebContents webContents);
    private static native boolean nativeIsCapturingScreen(WebContents webContents);
    private static native void nativeNotifyStopped(WebContents webContents);
}