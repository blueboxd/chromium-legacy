// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import android.support.annotation.IntDef;

/** Status after completion of a commit to storage. */
public final class CommitResult {
    /** IntDef that defines result values. */
    @IntDef({Result.SUCCESS, Result.FAILURE})
    public @interface Result {
        int SUCCESS = 0;
        int FAILURE = 1;
    }

    public @Result int getResult() {
        return mResult;
    }

    private final @Result int mResult;

    // Private constructor - use static instances
    private CommitResult(@Result int result) {
        this.mResult = result;
    }

    public static final CommitResult SUCCESS = new CommitResult(Result.SUCCESS);
    public static final CommitResult FAILURE = new CommitResult(Result.FAILURE);
}
