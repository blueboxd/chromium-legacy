// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.util.DisplayMetrics;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.feed.library.common.locale.LocaleUtils;

/**
 * Bridge for FeedService-related calls.
 */
@JNINamespace("feed")
public final class FeedServiceBridge {
    public static boolean isEnabled() {
        return FeedServiceBridgeJni.get().isEnabled();
    }

    // Java functionality needed for the native FeedService.
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

    @CalledByNative
    public static void clearAll() {
        FeedStreamSurface.clearAll();
    }

    /** Called at startup to trigger creation of |FeedService|. */
    public static void startup() {
        FeedServiceBridgeJni.get().startup();
    }

    public static String getClientInstanceId() {
        return FeedServiceBridgeJni.get().getClientInstanceId();
    }

    /** Retrieves the config value for load_more_trigger_lookahead. */
    public static int getLoadMoreTriggerLookahead() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerLookahead();
    }

    public static void reportOpenVisitComplete(long visitTimeMs) {
        FeedServiceBridgeJni.get().reportOpenVisitComplete(visitTimeMs);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled();
        void startup();
        int getLoadMoreTriggerLookahead();
        String getClientInstanceId();
        void reportOpenVisitComplete(long visitTimeMs);
    }
}
