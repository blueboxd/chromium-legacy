// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;

/**
 * A simple implementation of {@link NativePaintPreviewServiceProvider} used in tests.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTestService implements NativePaintPreviewServiceProvider {
    private static final String TAG = "PPTestService";
    private long mNativePaintPreviewTestService;

    public PaintPreviewTestService(String path) {
        mNativePaintPreviewTestService = PaintPreviewTestServiceJni.get().getInstance(path);
    }

    @Override
    public long getNativeService() {
        return mNativePaintPreviewTestService;
    }

    public boolean createSingleSkpForKey(
            String key, String url, int width, int height, Rect[] linkRects, String[] links) {
        if (mNativePaintPreviewTestService == 0) {
            Log.e(TAG, "No native service.");
            return false;
        }

        assert linkRects.length == links.length;

        int flattenedRects[] = new int[linkRects.length * 4];
        for (int i = 0; i < linkRects.length; i++) {
            flattenedRects[i * 4] = linkRects[i].left;
            flattenedRects[i * 4 + 1] = linkRects[i].top;
            flattenedRects[i * 4 + 2] = linkRects[i].width();
            flattenedRects[i * 4 + 3] = linkRects[i].height();
        }

        boolean ret = PaintPreviewTestServiceJni.get().createSingleSkpForKey(
                mNativePaintPreviewTestService, key, url, width, height, flattenedRects, links);
        if (!ret) {
            Log.e(TAG, "Native failed to setup files for testing.");
        }
        return ret;
    }

    @NativeMethods
    interface Natives {
        long getInstance(String path);
        boolean createSingleSkpForKey(long nativePaintPreviewTestService, String key, String url,
                int width, int height, int[] flattenedRects, String[] links);
    }
}
