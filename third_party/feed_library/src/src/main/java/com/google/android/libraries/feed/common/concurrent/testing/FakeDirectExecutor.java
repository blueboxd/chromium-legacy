// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent.testing;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

/** Fake {@link Executor} that enforces a background thread. */
public final class FakeDirectExecutor implements Executor {
    private final AtomicBoolean currentlyExecutingTasks = new AtomicBoolean();
    private final FakeThreadUtils fakeThreadUtils;
    private final List<Runnable> tasksToRun = new ArrayList<>();
    private final boolean shouldQueueTasks;

    public static FakeDirectExecutor runTasksImmediately(FakeThreadUtils fakeThreadUtils) {
        return new FakeDirectExecutor(fakeThreadUtils, /* shouldQueueTasks= */ false);
    }

    public static FakeDirectExecutor queueAllTasks(FakeThreadUtils fakeThreadUtils) {
        return new FakeDirectExecutor(fakeThreadUtils, /* shouldQueueTasks= */ true);
    }

    private FakeDirectExecutor(FakeThreadUtils fakeThreadUtils, boolean shouldQueueTasks) {
        this.fakeThreadUtils = fakeThreadUtils;
        this.shouldQueueTasks = shouldQueueTasks;
    }

    @Override
    public void execute(Runnable command) {
        tasksToRun.add(command);
        if (!shouldQueueTasks) {
            runAllTasks();
        }
    }

    public void runAllTasks() {
        if (currentlyExecutingTasks.getAndSet(true)) {
            return;
        }

        boolean policy = fakeThreadUtils.enforceMainThread(false);
        try {
            while (!tasksToRun.isEmpty()) {
                Runnable task = tasksToRun.remove(0);
                task.run();
            }
        } finally {
            fakeThreadUtils.enforceMainThread(policy);
            currentlyExecutingTasks.set(false);
        }
    }

    public boolean hasTasks() {
        return !tasksToRun.isEmpty();
    }
}
