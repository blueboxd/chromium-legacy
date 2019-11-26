// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.time.TimingUtils;

/**
 * Factory for creating a {@link InitializableSession} instance used in the {@link
 * com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager}.
 */
public final class SessionFactory {
    private final Store mStore;
    private final TaskQueue mTaskQueue;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final boolean mUseTimeScheduler;
    private final boolean mLimitPagingUpdates;
    private final boolean mLimitPagingUpdatesInHead;

    public SessionFactory(Store store, TaskQueue taskQueue, TimingUtils timingUtils,
            ThreadUtils threadUtils, Configuration config) {
        this.mStore = store;
        this.mTaskQueue = taskQueue;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        mUseTimeScheduler = config.getValueOrDefault(ConfigKey.USE_TIMEOUT_SCHEDULER, false);
        mLimitPagingUpdates = config.getValueOrDefault(ConfigKey.LIMIT_PAGE_UPDATES, true);
        mLimitPagingUpdatesInHead =
                config.getValueOrDefault(ConfigKey.LIMIT_PAGE_UPDATES_IN_HEAD, false);
    }

    public InitializableSession getSession() {
        return mUseTimeScheduler ? new TimeoutSessionImpl(
                       mStore, mLimitPagingUpdates, mTaskQueue, mTimingUtils, mThreadUtils)
                                 : new SessionImpl(mStore, mLimitPagingUpdates, mTaskQueue,
                                         mTimingUtils, mThreadUtils);
    }

    public HeadSessionImpl getHeadSession() {
        return new HeadSessionImpl(mStore, mTimingUtils, mLimitPagingUpdatesInHead);
    }
}
