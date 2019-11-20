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

package com.google.android.libraries.feed.api.internal.scope;

import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.lifecycle.Resettable;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * This class will implements Clear All in Jardin. It will run all of the clear operations on a
 * single background thread.
 */
public final class ClearAllListener implements FeedLifecycleListener, Dumpable {
  private static final String TAG = "ClearAllListener";

  private final TaskQueue taskQueue;
  private final FeedSessionManager feedSessionManager;
  private final /*@Nullable*/ Resettable store;
  private final ThreadUtils threadUtils;
  private int clearCount = 0;
  private int refreshCount = 0;

  @SuppressWarnings("argument.type.incompatible") // ok call to registerObserver
  public ClearAllListener(
      TaskQueue taskQueue,
      FeedSessionManager feedSessionManager,
      /*@Nullable*/ Resettable store,
      ThreadUtils threadUtils,
      FeedObservable<FeedLifecycleListener> lifecycleListenerObservable) {
    this.taskQueue = taskQueue;
    this.feedSessionManager = feedSessionManager;
    this.store = store;
    this.threadUtils = threadUtils;

    lifecycleListenerObservable.registerObserver(this);
  }

  @Override
  public void onLifecycleEvent(String event) {
    switch (event) {
      case LifecycleEvent.CLEAR_ALL:
        taskQueue.execute(Task.CLEAR_ALL, TaskType.IMMEDIATE, this::clearAll);
        break;
      case LifecycleEvent.CLEAR_ALL_WITH_REFRESH:
        taskQueue.execute(
            Task.CLEAR_ALL_WITH_REFRESH, TaskType.IMMEDIATE, this::clearAllWithRefresh);
        break;
      default:
        // Do nothing
    }
  }

  private void clearAll() {
    threadUtils.checkNotMainThread();
    clearCount++;

    Logger.i(TAG, "starting clearAll");
    // Clear the task queue first, preventing any tasks from running until initialization
    taskQueue.reset();
    // reset the session state
    feedSessionManager.reset();
    if (store != null) {
      store.reset();
    }
    // Initialize the TaskQueue so new tasks will start running
    taskQueue.completeReset();
  }

  private void clearAllWithRefresh() {
    threadUtils.checkNotMainThread();
    clearAll();
    feedSessionManager.triggerRefresh(
        null, RequestReason.CLEAR_ALL, UiContext.getDefaultInstance());
    refreshCount++;
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    dumper.forKey("clearCount").value(clearCount);
    dumper.forKey("clearWithRefreshCount").value(refreshCount);
  }
}
