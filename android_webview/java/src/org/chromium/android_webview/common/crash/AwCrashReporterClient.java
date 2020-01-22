// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.crash;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * A helper class for WebView-specific handling of Java crashes.
 */
@JNINamespace("android_webview")
public class AwCrashReporterClient {
    // The filename prefix used by GMS proguarding, which we use to recognise
    // otherwise colliding proguarded class names as belonging to dynamite
    // modules.
    private static final String DYNAMITE_PREFIX = ":com.google.android.gms";

    /**
     * Determine if a Throwable should be reported to the crash reporting mechanism.
     *
     * We report exceptions if any stack frame corresponds to a class directly defined in the
     * WebView classloader (which may have been proguarded) or is defined in an ancestral
     * classloader, but has package android.webkit. (i.e. it is a framework WebView class).
     * Technically we should also include androidx.webkit classes, but these are defined in the app
     * classloader, and may have been proguarded.
     *
     * @param t The throwable to check.
     * @return True if any frame of the stack trace matches the above rule.
     */
    @VisibleForTesting
    @CalledByNative
    public static boolean stackTraceContainsWebViewCode(Throwable t) {
        ClassLoader webViewClassLoader = AwCrashReporterClient.class.getClassLoader();
        for (StackTraceElement frame : t.getStackTrace()) {
            if (frame.getClassName().startsWith("android.webkit.")) {
                return true;
            }
            if (frame.getFileName().startsWith(DYNAMITE_PREFIX)) {
                continue;
            }
            try {
                Class frameClass = webViewClassLoader.loadClass(frame.getClassName());
                if (frameClass.getClassLoader() == webViewClassLoader) {
                    return true;
                }
            } catch (ClassNotFoundException e) {
            }
        }
        return false;
    }
}
