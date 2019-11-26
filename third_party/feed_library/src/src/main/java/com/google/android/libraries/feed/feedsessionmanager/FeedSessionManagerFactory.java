// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener;
import com.google.android.libraries.feed.feedsessionmanager.internal.ContentCache;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionCache;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionFactory;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionManagerMutation;

import java.util.concurrent.TimeUnit;

/**
 * Factory which creates the {@link FeedSessionManager}. This creates all the support classes before
 * creating the FeedSessionManager instead of creating the support objects inside the class
 * constructor.
 */
public final class FeedSessionManagerFactory {
    private static final long DEFAULT_LIFETIME_MS = TimeUnit.HOURS.toMillis(1L);

    private final TaskQueue mTaskQueue;
    private final Store mStore;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final ProtocolAdapter mProtocolAdapter;
    private final FeedRequestManager mFeedRequestManager;
    private final ActionUploadRequestManager mActionUploadRequestManager;
    private final SchedulerApi mSchedulerApi;
    private final Configuration mConfiguration;
    private final Clock mClock;
    private final FeedObservable<FeedLifecycleListener> mLifecycleListenerObservable;
    private final MainThreadRunner mMainThreadRunner;
    private final BasicLoggingApi mBasicLoggingApi;

    public FeedSessionManagerFactory(TaskQueue taskQueue, Store store, TimingUtils timingUtils,
            ThreadUtils threadUtils, ProtocolAdapter protocolAdapter,
            FeedRequestManager feedRequestManager,
            ActionUploadRequestManager actionUploadRequestManager, SchedulerApi schedulerApi,
            Configuration configuration, Clock clock,
            FeedObservable<FeedLifecycleListener> lifecycleListenerObservable,
            MainThreadRunner mainThreadRunner, BasicLoggingApi basicLoggingApi) {
        this.mTaskQueue = taskQueue;
        this.mStore = store;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mProtocolAdapter = protocolAdapter;
        this.mFeedRequestManager = feedRequestManager;
        this.mActionUploadRequestManager = actionUploadRequestManager;
        this.mSchedulerApi = schedulerApi;
        this.mConfiguration = configuration;
        this.mClock = clock;
        this.mLifecycleListenerObservable = lifecycleListenerObservable;
        this.mMainThreadRunner = mainThreadRunner;
        this.mBasicLoggingApi = basicLoggingApi;
    }

    /** Creates a new FeedSessionManager and initializes it */
    public FeedSessionManagerImpl create() {
        long lifetimeMs = mConfiguration.getValueOrDefault(
                ConfigKey.SESSION_LIFETIME_MS, DEFAULT_LIFETIME_MS);
        SessionFactory sessionFactory =
                new SessionFactory(mStore, mTaskQueue, mTimingUtils, mThreadUtils, mConfiguration);
        SessionCache sessionCache = new SessionCache(
                mStore, mTaskQueue, sessionFactory, lifetimeMs, mTimingUtils, mThreadUtils, mClock);
        ContentCache contentCache = new ContentCache();
        SessionManagerMutation sessionManagerMutation = new SessionManagerMutation(mStore,
                sessionCache, contentCache, mTaskQueue, mSchedulerApi, mThreadUtils, mTimingUtils,
                mClock, mMainThreadRunner, mBasicLoggingApi);

        return new FeedSessionManagerImpl(mTaskQueue, sessionFactory, sessionCache,
                sessionManagerMutation, contentCache, mStore, mTimingUtils, mThreadUtils,
                mProtocolAdapter, mFeedRequestManager, mActionUploadRequestManager, mSchedulerApi,
                mConfiguration, mClock, mLifecycleListenerObservable, mMainThreadRunner,
                mBasicLoggingApi);
    }
}
