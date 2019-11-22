// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedapplifecyclelistener;

import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;

/** Default implementation of {@link AppLifecycleListener} */
public final class FeedAppLifecycleListener
        extends FeedObservable<FeedLifecycleListener> implements AppLifecycleListener {
    private static final String TAG = "FeedAppLifecycleLstnr";

    private final ThreadUtils threadUtils;

    public FeedAppLifecycleListener(ThreadUtils threadUtils) {
        this.threadUtils = threadUtils;
    }

    @Override
    public void onEnterForeground() {
        threadUtils.checkMainThread();
        Logger.i(TAG, "onEnterForeground called");
        dispatchLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
    }

    @Override
    public void onEnterBackground() {
        threadUtils.checkMainThread();
        Logger.i(TAG, "onEnterBackground called");
        dispatchLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
    }

    @Override
    public void onClearAll() {
        Logger.i(TAG, "onClearAll called");
        threadUtils.checkMainThread();
        dispatchLifecycleEvent(LifecycleEvent.CLEAR_ALL);
    }

    @Override
    public void onClearAllWithRefresh() {
        threadUtils.checkMainThread();
        Logger.i(TAG, "onClearAllWithRefresh called");
        dispatchLifecycleEvent(LifecycleEvent.CLEAR_ALL_WITH_REFRESH);
    }

    @Override
    public void initialize() {
        threadUtils.checkMainThread();
        Logger.i(TAG, "initialize called");
        dispatchLifecycleEvent(LifecycleEvent.INITIALIZE);
    }

    private void dispatchLifecycleEvent(@LifecycleEvent String event) {
        synchronized (observers) {
            for (FeedLifecycleListener listener : observers) {
                listener.onLifecycleEvent(event);
            }
        }
    }
}
