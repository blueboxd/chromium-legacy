// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent.testing;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import org.chromium.chrome.browser.feed.library.common.concurrent.CancelableRunnableTask;
import org.chromium.chrome.browser.feed.library.common.concurrent.CancelableTask;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;

import java.util.ArrayList;
import java.util.List;
import java.util.PriorityQueue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A {@link MainThreadRunner} which listens to a {@link FakeClock} to determine when to execute
 * delayed tasks. This class can optionally execute tasks immediately and enforce thread checks.
 */
public final class FakeMainThreadRunner extends MainThreadRunner {
    private final AtomicBoolean mCurrentlyExecutingTasks = new AtomicBoolean();
    private final FakeClock mFakeClock;
    private final FakeThreadUtils mFakeThreadUtils;
    private final List<Runnable> mTasksToRun = new ArrayList<>();
    private final boolean mShouldQueueTasks;

    private final PriorityQueue<TimedRunnable> mDelayedTasks = new PriorityQueue<>(
            1, (a, b) -> Long.compare(a.getExecutionTime(), b.getExecutionTime()));

    private int mCompletedTaskCount;

    public static FakeMainThreadRunner create(FakeClock fakeClock) {
        return new FakeMainThreadRunner(
                fakeClock, FakeThreadUtils.withoutThreadChecks(), /* shouldQueueTasks= */ false);
    }

    public static FakeMainThreadRunner runTasksImmediately() {
        return create(new FakeClock());
    }

    public static FakeMainThreadRunner runTasksImmediatelyWithThreadChecks(
            FakeThreadUtils fakeThreadUtils) {
        return new FakeMainThreadRunner(
                new FakeClock(), fakeThreadUtils, /* shouldQueueTasks= */ false);
    }

    public static FakeMainThreadRunner queueAllTasks() {
        return new FakeMainThreadRunner(new FakeClock(), FakeThreadUtils.withoutThreadChecks(),
                /* shouldQueueTasks= */ true);
    }

    private FakeMainThreadRunner(
            FakeClock fakeClock, FakeThreadUtils fakeThreadUtils, boolean shouldQueueTasks) {
        this.mFakeClock = fakeClock;
        this.mFakeThreadUtils = fakeThreadUtils;
        this.mShouldQueueTasks = shouldQueueTasks;
        fakeClock.registerObserver(
                (newCurrentTime, newElapsedRealtime) -> runTasksBefore(newElapsedRealtime));
    }

    private void runTasksBefore(long newElapsedRealtime) {
        TimedRunnable nextTask;
        while ((nextTask = mDelayedTasks.peek()) != null) {
            if (nextTask.getExecutionTime() > newElapsedRealtime) {
                break;
            }

            Runnable task = checkNotNull(mDelayedTasks.poll());
            mTasksToRun.add(task);
        }

        if (!mShouldQueueTasks) {
            runAllTasks();
        }
    }

    @Override
    public void execute(String name, Runnable runnable) {
        mTasksToRun.add(runnable);
        if (!mShouldQueueTasks) {
            runAllTasks();
        }
    }

    @Override
    public CancelableTask executeWithDelay(String name, Runnable runnable, long delayMs) {
        CancelableRunnableTask cancelable = new CancelableRunnableTask(runnable);
        mDelayedTasks.add(new TimedRunnable(cancelable, mFakeClock.elapsedRealtime() + delayMs));
        return cancelable;
    }

    /** Runs all eligible tasks. */
    public void runAllTasks() {
        if (mCurrentlyExecutingTasks.getAndSet(true)) {
            return;
        }

        boolean policy = mFakeThreadUtils.enforceMainThread(true);
        try {
            while (!mTasksToRun.isEmpty()) {
                Runnable task = mTasksToRun.remove(0);
                task.run();
                mCompletedTaskCount++;
            }
        } finally {
            mFakeThreadUtils.enforceMainThread(policy);
            mCurrentlyExecutingTasks.set(false);
        }
    }

    /** Returns {@literal true} if there are tasks to run in the future. */
    public boolean hasPendingTasks() {
        if (!mTasksToRun.isEmpty()) {
            return true;
        }

        for (TimedRunnable runnable : mDelayedTasks) {
            if (!runnable.isCanceled()) {
                return true;
            }
        }

        return false;
    }

    /** Returns {@literal true} if there are tasks to run or tasks have run. */
    public boolean hasTasks() {
        return !mTasksToRun.isEmpty() || mCompletedTaskCount != 0;
    }

    /** Returns the number of tasks that have run. */
    public int getCompletedTaskCount() {
        return mCompletedTaskCount;
    }

    private static final class TimedRunnable implements Runnable {
        private final CancelableRunnableTask mRunnable;
        private final long mExecutionTime;

        private TimedRunnable(CancelableRunnableTask runnable, long executeTime) {
            this.mRunnable = runnable;
            this.mExecutionTime = executeTime;
        }

        private long getExecutionTime() {
            return mExecutionTime;
        }

        private boolean isCanceled() {
            return mRunnable.canceled();
        }

        @Override
        public void run() {
            mRunnable.run();
        }
    }
}
