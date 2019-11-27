// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import android.os.Looper;

/** Thread utilities. */
// TODO: Need to make this class file for Tiktok compliance.  This is mocked in a bunch
// of tests which run on the main thread.
public class ThreadUtils {
    public ThreadUtils() {}

    /** Returns {@code true} if this method is being called from the main/UI thread. */
    public boolean isMainThread() {
        return Looper.getMainLooper() == Looper.myLooper();
    }

    public void checkNotMainThread() {
        check(!isMainThread(), "checkNotMainThread failed");
    }

    public void checkMainThread() {
        check(isMainThread(), "checkMainThread failed");
    }

    protected void check(boolean condition, String message) {
        if (!condition) {
            throw new IllegalStateException(message);
        }
    }
}
