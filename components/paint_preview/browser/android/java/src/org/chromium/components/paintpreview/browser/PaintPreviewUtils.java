// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.browser;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper to capture paint previews via native.
 */
public class PaintPreviewUtils {
    /**
     * Captures a paint preview of the passed contents.
     * @param contents The WebContents of the page to capture.
     */
    public static void capturePaintPreview(WebContents contents) {
        PaintPreviewUtilsJni.get().capturePaintPreview(contents);
    }

    @NativeMethods
    interface Natives {
        void capturePaintPreview(WebContents webContents);
    }
}
