// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;

/**
 * A simple implementation of {@link NativePaintPreviewServiceProvider} used in tests.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTestService implements NativePaintPreviewServiceProvider {
    private long mNativePaintPreviewTestService;

    public PaintPreviewTestService(String testDataDir) {
        mNativePaintPreviewTestService = PaintPreviewTestServiceJni.get().getInstance(testDataDir);
    }

    @Override
    public long getNativeService() {
        return mNativePaintPreviewTestService;
    }

    @NativeMethods
    interface Natives {
        long getInstance(String testDataDir);
    }
}
