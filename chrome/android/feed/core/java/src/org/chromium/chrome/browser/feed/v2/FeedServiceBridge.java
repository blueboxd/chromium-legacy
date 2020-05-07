// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.util.DisplayMetrics;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.feed.library.common.locale.LocaleUtils;

// Java functionality needed for the native FeedService.
final class FeedServiceBridge {
    @CalledByNative
    public static String getLanguageTag() {
        return LocaleUtils.getLanguageTag(ContextUtils.getApplicationContext());
    }
    @CalledByNative
    public static double[] getDisplayMetrics() {
        DisplayMetrics metrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        double[] result = {metrics.density, metrics.widthPixels, metrics.heightPixels};
        return result;
    }
}
