// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.scope;

import android.content.Context;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.client.scope.ProcessScope;
import org.chromium.chrome.browser.feed.library.api.client.scope.StreamScopeBuilder;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;

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
    @Override
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
