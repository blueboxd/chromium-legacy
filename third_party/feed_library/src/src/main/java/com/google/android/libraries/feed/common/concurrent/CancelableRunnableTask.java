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
package com.google.android.libraries.feed.common.concurrent;

import java.util.concurrent.atomic.AtomicBoolean;

/** A thread safe runnable that can be canceled. */
public class CancelableRunnableTask implements CancelableTask, Runnable {
  private final AtomicBoolean canceled = new AtomicBoolean(false);
  private final Runnable runnable;

  public CancelableRunnableTask(Runnable runnable) {
    this.runnable = runnable;
  }

  @Override
  public void run() {
    if (!canceled.get()) {
      runnable.run();
    }
  }

  @Override
  public boolean canceled() {
    return canceled.get();
  }

  @Override
  public void cancel() {
    canceled.set(true);
  }
}
