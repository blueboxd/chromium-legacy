// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent.testing;

import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;

/** A fake {@link TaskQueue} implementation. */
public final class FakeTaskQueue extends TaskQueue {
    private boolean mResetWasCalled;
    private boolean mCompleteResetWasCalled;

    public FakeTaskQueue(FakeClock fakeClock, FakeThreadUtils fakeThreadUtils) {
        this(fakeClock, FakeDirectExecutor.runTasksImmediately(fakeThreadUtils));
    }

    public FakeTaskQueue(FakeClock fakeClock, FakeDirectExecutor fakeDirectExecutor) {
        super(new FakeBasicLoggingApi(), fakeDirectExecutor, FakeMainThreadRunner.create(fakeClock),
                fakeClock);
    }

    @Override
    public void reset() {
        super.reset();
        mResetWasCalled = true;
    }

    @Override
    public void completeReset() {
        super.completeReset();
        mCompleteResetWasCalled = true;
    }

    public void resetCounts() {
        mTaskCount = 0;
        mImmediateTaskCount = 0;
        mHeadInvalidateTaskCount = 0;
        mHeadResetTaskCount = 0;
        mUserFacingTaskCount = 0;
        mBackgroundTaskCount = 0;
    }

    public int getBackgroundTaskCount() {
        return mBackgroundTaskCount;
    }

    public int getImmediateTaskCount() {
        return mImmediateTaskCount;
    }

    public int getUserFacingTaskCount() {
        return mUserFacingTaskCount;
    }

    public boolean resetWasCalled() {
        return mResetWasCalled;
    }

    public boolean completeResetWasCalled() {
        return mCompleteResetWasCalled;
    }
}
