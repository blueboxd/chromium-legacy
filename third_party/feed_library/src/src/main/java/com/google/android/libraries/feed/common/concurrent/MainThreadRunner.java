// Copyright 2018 The Feed Authors.
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

package com.google.android.libraries.feed.common.concurrent;

import android.os.Handler;
import android.os.Looper;
import com.google.android.libraries.feed.common.logging.Logger;
import java.util.concurrent.Executor;

/** Executes a task on the main thread (UI thread) with optional delay. */
/*@DoNotMock("Use com.google.android.libraries.feed.common.concurrent.FakeMainThreadRunner instead")*/
public class MainThreadRunner {
  private static final String TAG = "MainThreadRunner";
  private static final Handler handler = new Handler(Looper.getMainLooper());

  private final Executor executor;

  public MainThreadRunner() {
    this.executor = handler::post;
  }

  /** Executes the {@code runnable} on the {@link Executor} used to initialize this class. */
  public void execute(String name, Runnable runnable) {
    Logger.i(TAG, "Running task [%s] on the Main Thread", name);
    executor.execute(runnable);
  }

  public CancelableTask executeWithDelay(String name, Runnable runnable, long delayMs) {
    CancelableRunnableTask cancelable = new CancelableRunnableTask(runnable);
    Logger.i(
        TAG, "Running task [%s] on the Main Thread with a delay of %d milliseconds", name, delayMs);
    handler.postDelayed(cancelable, delayMs);
    return cancelable;
  }
}
