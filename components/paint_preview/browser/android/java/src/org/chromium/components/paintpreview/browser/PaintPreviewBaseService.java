// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.browser;

import org.chromium.base.annotations.CalledByNative;

/**
 * The Java-side implementation of paint_preview_base_service.cc. This class is owned and managed by
 * its C++ counterpart.
 */
public class PaintPreviewBaseService {
    private long mNativePaintPreviewBaseService;

    @CalledByNative
    public PaintPreviewBaseService(long nativePaintPreviewBaseService) {
        mNativePaintPreviewBaseService = nativePaintPreviewBaseService;
    }

    @CalledByNative
    public void onDestroy() {
        mNativePaintPreviewBaseService = 0;
    }

    public long getNativePaintPreviewBaseService() {
        return mNativePaintPreviewBaseService;
    }
}