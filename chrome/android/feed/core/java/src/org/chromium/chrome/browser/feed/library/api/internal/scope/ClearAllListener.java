// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.scope;

import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.lifecycle.Resettable;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

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
