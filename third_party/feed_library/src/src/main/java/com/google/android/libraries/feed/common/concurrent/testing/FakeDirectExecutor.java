// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
