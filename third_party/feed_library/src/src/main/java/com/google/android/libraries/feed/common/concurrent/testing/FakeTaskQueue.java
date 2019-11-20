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

import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;

/** A fake {@link TaskQueue} implementation. */
public final class FakeTaskQueue extends TaskQueue {

  private boolean resetWasCalled = false;
  private boolean completeResetWasCalled = false;

  public FakeTaskQueue(FakeClock fakeClock, FakeThreadUtils fakeThreadUtils) {
    this(fakeClock, FakeDirectExecutor.runTasksImmediately(fakeThreadUtils));
  }

  public FakeTaskQueue(FakeClock fakeClock, FakeDirectExecutor fakeDirectExecutor) {
    super(
        new FakeBasicLoggingApi(),
        fakeDirectExecutor,
        FakeMainThreadRunner.create(fakeClock),
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
