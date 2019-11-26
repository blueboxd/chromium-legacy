// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.scope;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.proto.ProtoExtensionProvider;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalStorage;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionReader;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.api.internal.scope.ClearAllListener;
import com.google.android.libraries.feed.api.internal.scope.FeedProcessScope;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Validators;
import com.google.android.libraries.feed.common.concurrent.DirectHostSupported;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.SystemClockImpl;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.feedactionmanager.FeedActionManagerImpl;
import com.google.android.libraries.feed.feedactionreader.FeedActionReader;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedAppLifecycleListener;
import com.google.android.libraries.feed.feedknowncontent.FeedKnownContentImpl;
import com.google.android.libraries.feed.feedprotocoladapter.FeedProtocolAdapter;
import com.google.android.libraries.feed.feedrequestmanager.FeedActionUploadRequestManager;
import com.google.android.libraries.feed.feedrequestmanager.FeedRequestManagerImpl;
import com.google.android.libraries.feed.feedrequestmanager.RequestManagerImpl;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerFactory;
import com.google.android.libraries.feed.feedstore.ContentStorageDirectImpl;
import com.google.android.libraries.feed.feedstore.FeedStore;
import com.google.android.libraries.feed.feedstore.JournalStorageDirectImpl;
import com.google.android.libraries.feed.hostimpl.network.NetworkClientWrapper;
import com.google.android.libraries.feed.hostimpl.scheduler.SchedulerApiWrapper;
import com.google.android.libraries.feed.sharedstream.piet.PietRequiredContentAdapter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.Executor;

/** Creates an instance of {@link ProcessScope} */
public final class ProcessScopeBuilder {
    // Required fields.
    private final Configuration mConfiguration;
    private final Executor mSequencedExecutor;
    private final BasicLoggingApi mBasicLoggingApi;
    private final TooltipSupportedApi mTooltipSupportedApi;
    private final NetworkClient mUnwrappedNetworkClient;
    private final SchedulerApi mUnwrappedSchedulerApi;
    private final DebugBehavior mDebugBehavior;
    private final Context mContext;
    private final ApplicationInfo mApplicationInfo;

    // Optional fields - if they are not provided, we will use default implementations.
    /*@MonotonicNonNull*/ private ProtoExtensionProvider mProtoExtensionProvider;
    /*@MonotonicNonNull*/ private Clock mClock;

    // Either contentStorage or rawContentStorage must be provided.
    /*@MonotonicNonNull*/ ContentStorageDirect mContentStorage;
    /*@MonotonicNonNull*/ private ContentStorage mRawContentStorage;

    // Either journalStorage or rawJournalStorage must be provided.
    /*@MonotonicNonNull*/ JournalStorageDirect mJournalStorage;
    /*@MonotonicNonNull*/ private JournalStorage mRawJournalStorage;

    /** The APIs are all required to construct the scope. */
    public ProcessScopeBuilder(Configuration configuration, Executor sequencedExecutor,
            BasicLoggingApi basicLoggingApi, NetworkClient networkClient, SchedulerApi schedulerApi,
            DebugBehavior debugBehavior, Context context, ApplicationInfo applicationInfo,
            TooltipSupportedApi tooltipSupportedApi) {
        this.mConfiguration = configuration;
        this.mSequencedExecutor = sequencedExecutor;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mDebugBehavior = debugBehavior;
        this.mContext = context;
        this.mApplicationInfo = applicationInfo;
        this.mUnwrappedNetworkClient = networkClient;
        this.mUnwrappedSchedulerApi = schedulerApi;
        this.mTooltipSupportedApi = tooltipSupportedApi;
    }

    public ProcessScopeBuilder setProtoExtensionProvider(
            ProtoExtensionProvider protoExtensionProvider) {
        this.mProtoExtensionProvider = protoExtensionProvider;
        return this;
    }

    public ProcessScopeBuilder setContentStorage(ContentStorage contentStorage) {
        mRawContentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setContentStorageDirect(ContentStorageDirect contentStorage) {
        this.mContentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorage(JournalStorage journalStorage) {
        mRawJournalStorage = journalStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorageDirect(JournalStorageDirect journalStorage) {
        this.mJournalStorage = journalStorage;
        return this;
    }

    @VisibleForTesting
    ContentStorageDirect buildContentStorage(MainThreadRunner mainThreadRunner) {
        if (mContentStorage == null) {
            boolean useDirect =
                    mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && mRawContentStorage != null
                    && mRawContentStorage instanceof ContentStorageDirect) {
                mContentStorage = (ContentStorageDirect) mRawContentStorage;
            } else if (mRawContentStorage != null) {
                mContentStorage =
                        new ContentStorageDirectImpl(mRawContentStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of ContentStorage, ContentStorageDirect must be provided");
            }
        }
        return mContentStorage;
    }

    @VisibleForTesting
    JournalStorageDirect buildJournalStorage(MainThreadRunner mainThreadRunner) {
        if (mJournalStorage == null) {
            boolean useDirect =
                    mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && mRawJournalStorage != null
                    && mRawJournalStorage instanceof JournalStorageDirect) {
                mJournalStorage = (JournalStorageDirect) mRawJournalStorage;
            } else if (mRawJournalStorage != null) {
                mJournalStorage =
                        new JournalStorageDirectImpl(mRawJournalStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of JournalStorage, JournalStorageDirect must be provided");
            }
        }
        return mJournalStorage;
    }

    public ProcessScope build() {
        MainThreadRunner mainThreadRunner = new MainThreadRunner();
        mContentStorage = buildContentStorage(mainThreadRunner);
        mJournalStorage = buildJournalStorage(mainThreadRunner);

        ThreadUtils threadUtils = new ThreadUtils();

        boolean directHostCallEnabled =
                mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
        NetworkClient networkClient;
        SchedulerApi schedulerApi;
        if (mUnwrappedNetworkClient instanceof DirectHostSupported && directHostCallEnabled) {
            networkClient = mUnwrappedNetworkClient;
        } else {
            networkClient = new NetworkClientWrapper(
                    mUnwrappedNetworkClient, threadUtils, mainThreadRunner);
        }
        if (mUnwrappedSchedulerApi instanceof DirectHostSupported && directHostCallEnabled) {
            schedulerApi = mUnwrappedSchedulerApi;
        } else {
            schedulerApi =
                    new SchedulerApiWrapper(mUnwrappedSchedulerApi, threadUtils, mainThreadRunner);
        }

        // Build default component instances if necessary.
        if (mProtoExtensionProvider == null) {
            // Return an empty list of extensions by default.
            mProtoExtensionProvider = ArrayList::new;
        }
        FeedExtensionRegistry extensionRegistry =
                new FeedExtensionRegistry(mProtoExtensionProvider);
        if (mClock == null) {
            mClock = new SystemClockImpl();
        }
        TimingUtils timingUtils = new TimingUtils();
        TaskQueue taskQueue =
                new TaskQueue(mBasicLoggingApi, mSequencedExecutor, mainThreadRunner, mClock);
        FeedStore store = new FeedStore(mConfiguration, timingUtils, extensionRegistry,
                mContentStorage, mJournalStorage, threadUtils, taskQueue, mClock, mBasicLoggingApi,
                mainThreadRunner);

        FeedAppLifecycleListener lifecycleListener = new FeedAppLifecycleListener(threadUtils);
        lifecycleListener.registerObserver(store);

        ProtocolAdapter protocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        ActionReader actionReader =
                new FeedActionReader(store, mClock, protocolAdapter, taskQueue, mConfiguration);
        FeedRequestManager feedRequestManager = new FeedRequestManagerImpl(mConfiguration,
                networkClient, protocolAdapter, extensionRegistry, schedulerApi, taskQueue,
                timingUtils, threadUtils, actionReader, mContext, mApplicationInfo,
                mainThreadRunner, mBasicLoggingApi, mTooltipSupportedApi);
        ActionUploadRequestManager actionUploadRequestManager =
                new FeedActionUploadRequestManager(mConfiguration, networkClient, protocolAdapter,
                        extensionRegistry, taskQueue, threadUtils, store, mClock);
        FeedSessionManagerFactory fsmFactory = new FeedSessionManagerFactory(taskQueue, store,
                timingUtils, threadUtils, protocolAdapter, feedRequestManager,
                actionUploadRequestManager, schedulerApi, mConfiguration, mClock, lifecycleListener,
                mainThreadRunner, mBasicLoggingApi);
        FeedSessionManager feedSessionManager = fsmFactory.create();
        RequestManagerImpl clientRequestManager =
                new RequestManagerImpl(feedRequestManager, feedSessionManager);
        ActionManager actionManager = new FeedActionManagerImpl(
                feedSessionManager, store, threadUtils, taskQueue, mainThreadRunner, mClock);

        FeedKnownContent feedKnownContent =
                new FeedKnownContentImpl(feedSessionManager, mainThreadRunner, threadUtils);

        ClearAllListener clearAllListener = new ClearAllListener(
                taskQueue, feedSessionManager, store, threadUtils, lifecycleListener);
        return new FeedProcessScope(mBasicLoggingApi, networkClient,
                Validators.checkNotNull(protocolAdapter),
                Validators.checkNotNull(clientRequestManager),
                Validators.checkNotNull(feedSessionManager), store, timingUtils, threadUtils,
                taskQueue, mainThreadRunner, lifecycleListener, mClock, mDebugBehavior,
                actionManager, mConfiguration, feedKnownContent, extensionRegistry,
                clearAllListener, mTooltipSupportedApi, mApplicationInfo);
    }
}
