// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.scope;

import android.content.Context;

import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.api.client.scope.StreamScopeBuilder;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;

/**
 * Per-process instance of the feed library.
 *
 * <p>It's the host's responsibility to make sure there's only one instance of this per process, per
 * user.
 */
public final class FeedProcessScope implements ProcessScope {
    private static final String TAG = "FeedProcessScope";

    private final NetworkClient mNetworkClient;
    private final ProtocolAdapter mProtocolAdapter;
    private final RequestManager mRequestManager;
    private final FeedSessionManager mFeedSessionManager;
    private final Store mStore;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final TaskQueue mTaskQueue;
    private final MainThreadRunner mMainThreadRunner;
    private final AppLifecycleListener mAppLifecycleListener;
    private final Clock mClock;
    private final DebugBehavior mDebugBehavior;
    private final ActionManager mActionManager;
    private final Configuration mConfiguration;
    private final FeedKnownContent mFeedKnownContent;
    private final FeedExtensionRegistry mFeedExtensionRegistry;
    private final ClearAllListener mClearAllListener;
    private final BasicLoggingApi mBasicLoggingApi;
    private final TooltipSupportedApi mTooltipSupportedApi;
    private final ApplicationInfo mApplicationInfo;

    /** Created through the {@link Builder}. */
    public FeedProcessScope(BasicLoggingApi basicLoggingApi, NetworkClient networkClient,
            ProtocolAdapter protocolAdapter, RequestManager requestManager,
            FeedSessionManager feedSessionManager, Store store, TimingUtils timingUtils,
            ThreadUtils threadUtils, TaskQueue taskQueue, MainThreadRunner mainThreadRunner,
            AppLifecycleListener appLifecycleListener, Clock clock, DebugBehavior debugBehavior,
            ActionManager actionManager, Configuration configuration,
            FeedKnownContent feedKnownContent, FeedExtensionRegistry feedExtensionRegistry,
            ClearAllListener clearAllListener, TooltipSupportedApi tooltipSupportedApi,
            ApplicationInfo applicationInfo) {
        this.mBasicLoggingApi = basicLoggingApi;
        this.mNetworkClient = networkClient;
        this.mProtocolAdapter = protocolAdapter;
        this.mRequestManager = requestManager;
        this.mFeedSessionManager = feedSessionManager;
        this.mStore = store;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mTaskQueue = taskQueue;
        this.mMainThreadRunner = mainThreadRunner;
        this.mAppLifecycleListener = appLifecycleListener;
        this.mClock = clock;
        this.mDebugBehavior = debugBehavior;
        this.mActionManager = actionManager;
        this.mConfiguration = configuration;
        this.mFeedKnownContent = feedKnownContent;
        this.mFeedExtensionRegistry = feedExtensionRegistry;
        this.mClearAllListener = clearAllListener;
        this.mTooltipSupportedApi = tooltipSupportedApi;
        this.mApplicationInfo = applicationInfo;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        if (mProtocolAdapter instanceof Dumpable) {
            dumper.dump((Dumpable) mProtocolAdapter);
        }
        dumper.dump(mTimingUtils);
        if (mFeedSessionManager instanceof Dumpable) {
            dumper.dump((Dumpable) mFeedSessionManager);
        }
        if (mStore instanceof Dumpable) {
            dumper.dump((Dumpable) mStore);
        }
        dumper.dump(mClearAllListener);
    }

    @Override
    public void onDestroy() {
        try {
            Logger.i(TAG, "FeedProcessScope onDestroy called");
            mNetworkClient.close();
            mTaskQueue.reset();
            mTaskQueue.completeReset();
        } catch (Exception ignored) {
            // Ignore exception when closing.
        }
    }

    @Deprecated
    public ProtocolAdapter getProtocolAdapter() {
        return mProtocolAdapter;
    }

    @Override
    public RequestManager getRequestManager() {
        return mRequestManager;
    }

    @Deprecated
    public TimingUtils getTimingUtils() {
        return mTimingUtils;
    }

    @Override
    public TaskQueue getTaskQueue() {
        return mTaskQueue;
    }

    @Override
    public AppLifecycleListener getAppLifecycleListener() {
        return mAppLifecycleListener;
    }

    @Deprecated
    public ActionManager getActionManager() {
        return mActionManager;
    }

    public KnownContent getKnownContent() {
        return mFeedKnownContent;
    }

    @Deprecated
    public FeedExtensionRegistry getFeedExtensionRegistry() {
        return mFeedExtensionRegistry;
    }

    /**
     * Return a {@link Builder} to create a FeedProcessScope
     *
     * <p>This is called by hosts so it must be public
     */
    @Override
    public StreamScopeBuilder createStreamScopeBuilder(Context context,
            ImageLoaderApi imageLoaderApi, ActionApi actionApi,
            StreamConfiguration streamConfiguration, CardConfiguration cardConfiguration,
            SnackbarApi snackbarApi, OfflineIndicatorApi offlineIndicatorApi,
            TooltipApi tooltipApi) {
        return new StreamScopeBuilder(context, actionApi, imageLoaderApi, mProtocolAdapter,
                mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue, mMainThreadRunner,
                mClock, mDebugBehavior, streamConfiguration, cardConfiguration, mActionManager,
                mConfiguration, snackbarApi, mBasicLoggingApi, offlineIndicatorApi,
                mFeedKnownContent, tooltipApi, mTooltipSupportedApi, mApplicationInfo,
                mFeedExtensionRegistry);
    }
}
