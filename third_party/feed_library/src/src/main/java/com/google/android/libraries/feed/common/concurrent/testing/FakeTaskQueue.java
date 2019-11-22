// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent.testing;

import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;

/** A fake {@link TaskQueue} implementation. */
public final class FakeTaskQueue extends TaskQueue {
    private boolean resetWasCalled;
    private boolean completeResetWasCalled;

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
        resetWasCalled = true;
    }

    @Override
    public void completeReset() {
        super.completeReset();
        completeResetWasCalled = true;
    }

    public void resetCounts() {
        taskCount = 0;
        immediateTaskCount = 0;
        headInvalidateTaskCount = 0;
        headResetTaskCount = 0;
        userFacingTaskCount = 0;
        backgroundTaskCount = 0;
    }

    public int getBackgroundTaskCount() {
        return backgroundTaskCount;
    }

    public int getImmediateTaskCount() {
        return immediateTaskCount;
    }

    public int getUserFacingTaskCount() {
        return userFacingTaskCount;
    }

    public boolean resetWasCalled() {
        return resetWasCalled;
    }

    public boolean completeResetWasCalled() {
        return completeResetWasCalled;
    }
}
