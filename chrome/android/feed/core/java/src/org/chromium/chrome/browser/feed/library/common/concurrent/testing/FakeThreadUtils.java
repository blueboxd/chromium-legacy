// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent.testing;

import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;

/** Fake implements of {@link ThreadUtils} that allows callers to control the thread policy. */
public final class FakeThreadUtils extends ThreadUtils {
    private boolean mIsMainThread = true;
    private boolean mEnforceThreadChecks = true;

    private FakeThreadUtils(boolean enforceThreadChecks) {
        this.mEnforceThreadChecks = enforceThreadChecks;
    }

    @Override
    public boolean isMainThread() {
        return mIsMainThread;
    }

    @Override
    protected void check(boolean condition, String message) {
        if (mEnforceThreadChecks) {
            super.check(condition, message);
        }
    }

    /**
     * Updates whether the current execution is considered to be on the main thread. Returns the
     * existing thread policy.
     */
    public boolean enforceMainThread(boolean enforceMainThread) {
        boolean result = mIsMainThread;
        mIsMainThread = enforceMainThread;
        return result;
    }

    /** Create a {@link FakeThreadUtils} with thread enforcement. */
    public static FakeThreadUtils withThreadChecks() {
        return new FakeThreadUtils(/* enforceThreadChecks= */ true);
    }

    /** Create a {@link FakeThreadUtils} without thread enforcement. */
    public static FakeThreadUtils withoutThreadChecks() {
        return new FakeThreadUtils(/* enforceThreadChecks= */ false);
    }
}
