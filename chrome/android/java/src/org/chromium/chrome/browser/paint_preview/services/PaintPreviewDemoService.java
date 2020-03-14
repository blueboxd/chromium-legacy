// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java-side implementations of paint_preview_demo_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation.
 * This class provides the required functionalities for capturing and playing back the Paint Preview
 * representation of tab for the purpose of a demoing the Paint Preview component.
 */
@JNINamespace("paint_preview")
public class PaintPreviewDemoService implements NativePaintPreviewServiceProvider {
    private long mNativePaintPreviewDemoService;

    @CalledByNative
    private PaintPreviewDemoService(long nativePaintPreviewDemoService) {
        mNativePaintPreviewDemoService = nativePaintPreviewDemoService;
    }

    @CalledByNative
    private void destroy() {
        mNativePaintPreviewDemoService = 0;
    }

    @Override
    public long getNativeService() {
        return mNativePaintPreviewDemoService;
    }

    public void capturePaintPreview(
            WebContents webContents, int tabId, Callback<Boolean> successCallback) {
        if (mNativePaintPreviewDemoService == 0) {
            successCallback.onResult(false);
            return;
        }

        PaintPreviewDemoServiceJni.get().capturePaintPreviewJni(
                mNativePaintPreviewDemoService, webContents, tabId, successCallback);
    }

    public void cleanUpForTabId(int tabId) {
        if (mNativePaintPreviewDemoService == 0) {
            return;
        }

        PaintPreviewDemoServiceJni.get().cleanUpForTabId(mNativePaintPreviewDemoService, tabId);
    }

    @NativeMethods
    interface Natives {
        void capturePaintPreviewJni(long nativePaintPreviewDemoService, WebContents webContents,
                int tabId, Callback<Boolean> successCallback);
        void cleanUpForTabId(long nativePaintPreviewDemoService, int tabId);
    }
}
