// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;

/**
 * A Java API for requesting an Uri from a bitmap.
 */
public class BitmapUriRequest {
    public static String bitmapUri(Bitmap bitmap) {
        return BitmapUriRequestJni.get().bitmapUri(bitmap);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        String bitmapUri(Bitmap bitmap);
    }
}
