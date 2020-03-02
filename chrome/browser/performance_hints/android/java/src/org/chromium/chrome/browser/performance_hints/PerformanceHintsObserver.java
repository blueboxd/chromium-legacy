// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.performance_hints;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provide access to PerformanceClass.
 */
public class PerformanceHintsObserver {
    // From components/optimization_guide/proto/performance_hints_metadata.proto:PerformanceClass.
    @IntDef({PerformanceClass.PERFORMANCE_UNKNOWN, PerformanceClass.PERFORMANCE_SLOW,
            PerformanceClass.PERFORMANCE_FAST, PerformanceClass.PERFORMANCE_NORMAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PerformanceClass {
        int PERFORMANCE_UNKNOWN = 0;
        int PERFORMANCE_SLOW = 1;
        int PERFORMANCE_FAST = 2;
        int PERFORMANCE_NORMAL = 3;
    }

    public static @PerformanceClass int getPerformanceClassForURL(
            WebContents webContents, String url) {
        return PerformanceHintsObserverJni.get().getPerformanceClassForURL(webContents, url);
    }

    private PerformanceHintsObserver() {}

    @NativeMethods
    interface Natives {
        int getPerformanceClassForURL(WebContents webContents, String url);
    }
}
