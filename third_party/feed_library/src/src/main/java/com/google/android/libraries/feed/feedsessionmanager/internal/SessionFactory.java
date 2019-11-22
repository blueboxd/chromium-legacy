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
    private final Store store;
    private final TaskQueue taskQueue;
    private final TimingUtils timingUtils;
    private final ThreadUtils threadUtils;
    private final boolean useTimeScheduler;
    private final boolean limitPagingUpdates;
    private final boolean limitPagingUpdatesInHead;

    public SessionFactory(Store store, TaskQueue taskQueue, TimingUtils timingUtils,
            ThreadUtils threadUtils, Configuration config) {
        this.store = store;
        this.taskQueue = taskQueue;
        this.timingUtils = timingUtils;
        this.threadUtils = threadUtils;
        useTimeScheduler = config.getValueOrDefault(ConfigKey.USE_TIMEOUT_SCHEDULER, false);
        limitPagingUpdates = config.getValueOrDefault(ConfigKey.LIMIT_PAGE_UPDATES, true);
        limitPagingUpdatesInHead =
                config.getValueOrDefault(ConfigKey.LIMIT_PAGE_UPDATES_IN_HEAD, false);
    }

    public InitializableSession getSession() {
        return useTimeScheduler
                ? new TimeoutSessionImpl(
                        store, limitPagingUpdates, taskQueue, timingUtils, threadUtils)
                : new SessionImpl(store, limitPagingUpdates, taskQueue, timingUtils, threadUtils);
    }

    public HeadSessionImpl getHeadSession() {
        return new HeadSessionImpl(store, timingUtils, limitPagingUpdatesInHead);
    }
}
