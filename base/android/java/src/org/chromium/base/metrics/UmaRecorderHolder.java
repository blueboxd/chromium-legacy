// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.annotation.VisibleForTesting;

/** Holds the {@link CachingUmaRecorder} used by {@link RecordHistogram}. */
public class UmaRecorderHolder {
    private UmaRecorderHolder() {}

    /** The instance held by this class. */
    private static CachingUmaRecorder sRecorder = new CachingUmaRecorder();

    /** Returns the {@link CachingUmaRecorder}. */
    /* package */ static CachingUmaRecorder get() {
        return sRecorder;
    }

    /** Starts forwarding metrics to the native code. Returns after the cache has been flushed. */
    public static void onLibraryLoaded() {
        sRecorder.setDelegate(new NativeUmaRecorder());
    }

    /**
     * Tests may need to disable metrics. The value should be reset after the test done, to avoid
     * carrying over state to unrelated tests.
     *
     * @deprecated This method does nothing.
     */
    @VisibleForTesting
    @Deprecated
    public static void setDisabledForTests(boolean disabled) {}
}
