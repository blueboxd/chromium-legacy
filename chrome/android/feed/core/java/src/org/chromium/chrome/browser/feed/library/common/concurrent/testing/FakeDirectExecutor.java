// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent.testing;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

/** Fake {@link Executor} that enforces a background thread. */
public final class FakeDirectExecutor implements Executor {
    private final AtomicBoolean mCurrentlyExecutingTasks = new AtomicBoolean();
    private final FakeThreadUtils mFakeThreadUtils;
    private final List<Runnable> mTasksToRun = new ArrayList<>();
    private final boolean mShouldQueueTasks;

    public static FakeDirectExecutor runTasksImmediately(FakeThreadUtils fakeThreadUtils) {
        return new FakeDirectExecutor(fakeThreadUtils, /* shouldQueueTasks= */ false);
    }

    public static FakeDirectExecutor queueAllTasks(FakeThreadUtils fakeThreadUtils) {
        return new FakeDirectExecutor(fakeThreadUtils, /* shouldQueueTasks= */ true);
    }

    private FakeDirectExecutor(FakeThreadUtils fakeThreadUtils, boolean shouldQueueTasks) {
        this.mFakeThreadUtils = fakeThreadUtils;
        this.mShouldQueueTasks = shouldQueueTasks;
    }

    @Override
    public void execute(Runnable command) {
        mTasksToRun.add(command);
        if (!mShouldQueueTasks) {
            runAllTasks();
        }
    }

    public void runAllTasks() {
        if (mCurrentlyExecutingTasks.getAndSet(true)) {
            return;
        }

        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        try {
            while (!mTasksToRun.isEmpty()) {
                Runnable task = mTasksToRun.remove(0);
                task.run();
            }
        } finally {
            mFakeThreadUtils.enforceMainThread(policy);
            mCurrentlyExecutingTasks.set(false);
        }
    }

    public boolean hasTasks() {
        return !mTasksToRun.isEmpty();
    }
}
