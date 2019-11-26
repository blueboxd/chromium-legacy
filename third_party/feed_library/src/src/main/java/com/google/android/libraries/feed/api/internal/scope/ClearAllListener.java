// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    private final TaskQueue mTaskQueue;
    private final FeedSessionManager mFeedSessionManager;
    private final /*@Nullable*/ Resettable mStore;
    private final ThreadUtils mThreadUtils;
    private int mClearCount;
    private int mRefreshCount;

    @SuppressWarnings("argument.type.incompatible") // ok call to registerObserver
    public ClearAllListener(TaskQueue taskQueue, FeedSessionManager feedSessionManager,
            /*@Nullable*/ Resettable store, ThreadUtils threadUtils,
            FeedObservable<FeedLifecycleListener> lifecycleListenerObservable) {
        this.mTaskQueue = taskQueue;
        this.mFeedSessionManager = feedSessionManager;
        this.mStore = store;
        this.mThreadUtils = threadUtils;

        lifecycleListenerObservable.registerObserver(this);
    }

    @Override
    public void onLifecycleEvent(String event) {
        switch (event) {
            case LifecycleEvent.CLEAR_ALL:
                mTaskQueue.execute(Task.CLEAR_ALL, TaskType.IMMEDIATE, this::clearAll);
                break;
            case LifecycleEvent.CLEAR_ALL_WITH_REFRESH:
                mTaskQueue.execute(
                        Task.CLEAR_ALL_WITH_REFRESH, TaskType.IMMEDIATE, this::clearAllWithRefresh);
                break;
            default:
                // Do nothing
        }
    }

    private void clearAll() {
        mThreadUtils.checkNotMainThread();
        mClearCount++;

        Logger.i(TAG, "starting clearAll");
        // Clear the task queue first, preventing any tasks from running until initialization
        mTaskQueue.reset();
        // reset the session state
        mFeedSessionManager.reset();
        if (mStore != null) {
            mStore.reset();
        }
        // Initialize the TaskQueue so new tasks will start running
        mTaskQueue.completeReset();
    }

    private void clearAllWithRefresh() {
        mThreadUtils.checkNotMainThread();
        clearAll();
        mFeedSessionManager.triggerRefresh(
                null, RequestReason.CLEAR_ALL, UiContext.getDefaultInstance());
        mRefreshCount++;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("clearCount").value(mClearCount);
        dumper.forKey("clearWithRefreshCount").value(mRefreshCount);
    }
}
