// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;

/**
 * Factory for creating a {@link InitializableSession} instance used in the {@link
 * org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager}.
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
