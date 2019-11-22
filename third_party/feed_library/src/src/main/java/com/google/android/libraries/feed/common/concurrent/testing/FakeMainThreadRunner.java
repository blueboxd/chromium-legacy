// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent.testing;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import com.google.android.libraries.feed.common.concurrent.CancelableRunnableTask;
import com.google.android.libraries.feed.common.concurrent.CancelableTask;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.time.testing.FakeClock;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A {@link MainThreadRunner} which listens to a {@link FakeClock} to determine when to execute
 * delayed tasks. This class can optionally execute tasks immediately and enforce thread checks.
 */
public final class FakeMainThreadRunner extends MainThreadRunner {
    private final AtomicBoolean currentlyExecutingTasks = new AtomicBoolean();
    private final FakeClock fakeClock;
    private final FakeThreadUtils fakeThreadUtils;
    private final List<Runnable> tasksToRun = new ArrayList<>();
    private final boolean shouldQueueTasks;

    // Suppressing use of Comparator, which is fine because this is only used in tests.
    @SuppressWarnings("AndroidJdkLibsChecker")
    private final PriorityQueue<TimedRunnable> delayedTasks =
            new PriorityQueue<>(Comparator.comparingLong(TimedRunnable::getExecutionTime));

    private int completedTaskCount;

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
        this.fakeClock = fakeClock;
        this.fakeThreadUtils = fakeThreadUtils;
        this.shouldQueueTasks = shouldQueueTasks;
        fakeClock.registerObserver(
                (newCurrentTime, newElapsedRealtime) -> runTasksBefore(newElapsedRealtime));
    }

    private void runTasksBefore(long newElapsedRealtime) {
        TimedRunnable nextTask;
        while ((nextTask = delayedTasks.peek()) != null) {
            if (nextTask.getExecutionTime() > newElapsedRealtime) {
                break;
            }

            Runnable task = checkNotNull(delayedTasks.poll());
            tasksToRun.add(task);
        }

        if (!shouldQueueTasks) {
            runAllTasks();
        }
    }

    @Override
    public void execute(String name, Runnable runnable) {
        tasksToRun.add(runnable);
        if (!shouldQueueTasks) {
            runAllTasks();
        }
    }

    @Override
    public CancelableTask executeWithDelay(String name, Runnable runnable, long delayMs) {
        CancelableRunnableTask cancelable = new CancelableRunnableTask(runnable);
        delayedTasks.add(new TimedRunnable(cancelable, fakeClock.elapsedRealtime() + delayMs));
        return cancelable;
    }

    /** Runs all eligible tasks. */
    public void runAllTasks() {
        if (currentlyExecutingTasks.getAndSet(true)) {
            return;
        }

        boolean policy = fakeThreadUtils.enforceMainThread(true);
        try {
            while (!tasksToRun.isEmpty()) {
                Runnable task = tasksToRun.remove(0);
                task.run();
                completedTaskCount++;
            }
        } finally {
            fakeThreadUtils.enforceMainThread(policy);
            currentlyExecutingTasks.set(false);
        }
    }

    /** Returns {@literal true} if there are tasks to run in the future. */
    public boolean hasPendingTasks() {
        if (!tasksToRun.isEmpty()) {
            return true;
        }

        for (TimedRunnable runnable : delayedTasks) {
            if (!runnable.isCanceled()) {
                return true;
            }
        }

        return false;
    }

    /** Returns {@literal true} if there are tasks to run or tasks have run. */
    public boolean hasTasks() {
        return !tasksToRun.isEmpty() || completedTaskCount != 0;
    }

    /** Returns the number of tasks that have run. */
    public int getCompletedTaskCount() {
        return completedTaskCount;
    }

    private static final class TimedRunnable implements Runnable {
        private final CancelableRunnableTask runnable;
        private final long executionTime;

        private TimedRunnable(CancelableRunnableTask runnable, long executeTime) {
            this.runnable = runnable;
            this.executionTime = executeTime;
        }

        private long getExecutionTime() {
            return executionTime;
        }

        private boolean isCanceled() {
            return runnable.canceled();
        }

        @Override
        public void run() {
            runnable.run();
        }
    }
}
