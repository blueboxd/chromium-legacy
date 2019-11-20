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

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;
import java.util.concurrent.ExecutionException;

/** Abstract class which support calling Host methods on the {@link MainThreadRunner}. */
public abstract class MainThreadCaller {
  private static final String TAG = "MainThreadCaller";
  private final MainThreadRunner mainThreadRunner;

  protected MainThreadCaller(MainThreadRunner mainThreadRunner) {
    this.mainThreadRunner = mainThreadRunner;
  }

  /** Execute a task with a {@link Consumer}. */
  protected interface ConsumerTask<T> {
    void execute(Consumer<T> consumer);
  }

  /** Run a {@link ConsumerTask} on the main thread, providing a Consumer to receive the results. */
  protected <T> T mainThreadCaller(String location, ConsumerTask<T> task, T failure) {
    SimpleSettableFuture<T> sharedStatesFuture = new SimpleSettableFuture<>();
    mainThreadRunner.execute(TAG + " " + location, () -> task.execute(sharedStatesFuture::put));
    try {
      return sharedStatesFuture.get();
    } catch (InterruptedException | ExecutionException e) {
      Logger.e(TAG, e, null);
      return failure;
    }
  }
}
