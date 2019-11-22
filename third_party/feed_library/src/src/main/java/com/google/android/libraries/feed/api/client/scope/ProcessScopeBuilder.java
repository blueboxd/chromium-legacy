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
    private final Configuration configuration;
    private final Executor sequencedExecutor;
    private final BasicLoggingApi basicLoggingApi;
    private final TooltipSupportedApi tooltipSupportedApi;
    private final NetworkClient unwrappedNetworkClient;
    private final SchedulerApi unwrappedSchedulerApi;
    private final DebugBehavior debugBehavior;
    private final Context context;
    private final ApplicationInfo applicationInfo;

    // Optional fields - if they are not provided, we will use default implementations.
    /*@MonotonicNonNull*/ private ProtoExtensionProvider protoExtensionProvider;
    /*@MonotonicNonNull*/ private Clock clock;

    // Either contentStorage or rawContentStorage must be provided.
    /*@MonotonicNonNull*/ ContentStorageDirect contentStorage;
    /*@MonotonicNonNull*/ private ContentStorage rawContentStorage;

    // Either journalStorage or rawJournalStorage must be provided.
    /*@MonotonicNonNull*/ JournalStorageDirect journalStorage;
    /*@MonotonicNonNull*/ private JournalStorage rawJournalStorage;

    /** The APIs are all required to construct the scope. */
    public ProcessScopeBuilder(Configuration configuration, Executor sequencedExecutor,
            BasicLoggingApi basicLoggingApi, NetworkClient networkClient, SchedulerApi schedulerApi,
            DebugBehavior debugBehavior, Context context, ApplicationInfo applicationInfo,
            TooltipSupportedApi tooltipSupportedApi) {
        this.configuration = configuration;
        this.sequencedExecutor = sequencedExecutor;
        this.basicLoggingApi = basicLoggingApi;
        this.debugBehavior = debugBehavior;
        this.context = context;
        this.applicationInfo = applicationInfo;
        this.unwrappedNetworkClient = networkClient;
        this.unwrappedSchedulerApi = schedulerApi;
        this.tooltipSupportedApi = tooltipSupportedApi;
    }

    public ProcessScopeBuilder setProtoExtensionProvider(
            ProtoExtensionProvider protoExtensionProvider) {
        this.protoExtensionProvider = protoExtensionProvider;
        return this;
    }

    public ProcessScopeBuilder setContentStorage(ContentStorage contentStorage) {
        rawContentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setContentStorageDirect(ContentStorageDirect contentStorage) {
        this.contentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorage(JournalStorage journalStorage) {
        rawJournalStorage = journalStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorageDirect(JournalStorageDirect journalStorage) {
        this.journalStorage = journalStorage;
        return this;
    }

    @VisibleForTesting
    ContentStorageDirect buildContentStorage(MainThreadRunner mainThreadRunner) {
        if (contentStorage == null) {
            boolean useDirect =
                    configuration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && rawContentStorage != null
                    && rawContentStorage instanceof ContentStorageDirect) {
                contentStorage = (ContentStorageDirect) rawContentStorage;
            } else if (rawContentStorage != null) {
                contentStorage = new ContentStorageDirectImpl(rawContentStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of ContentStorage, ContentStorageDirect must be provided");
            }
        }
        return contentStorage;
    }

    @VisibleForTesting
    JournalStorageDirect buildJournalStorage(MainThreadRunner mainThreadRunner) {
        if (journalStorage == null) {
            boolean useDirect =
                    configuration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && rawJournalStorage != null
                    && rawJournalStorage instanceof JournalStorageDirect) {
                journalStorage = (JournalStorageDirect) rawJournalStorage;
            } else if (rawJournalStorage != null) {
                journalStorage = new JournalStorageDirectImpl(rawJournalStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of JournalStorage, JournalStorageDirect must be provided");
            }
        }
        return journalStorage;
    }

    public ProcessScope build() {
        MainThreadRunner mainThreadRunner = new MainThreadRunner();
        contentStorage = buildContentStorage(mainThreadRunner);
        journalStorage = buildJournalStorage(mainThreadRunner);

        ThreadUtils threadUtils = new ThreadUtils();

        boolean directHostCallEnabled =
                configuration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
        NetworkClient networkClient;
        SchedulerApi schedulerApi;
        if (unwrappedNetworkClient instanceof DirectHostSupported && directHostCallEnabled) {
            networkClient = unwrappedNetworkClient;
        } else {
            networkClient =
                    new NetworkClientWrapper(unwrappedNetworkClient, threadUtils, mainThreadRunner);
        }
        if (unwrappedSchedulerApi instanceof DirectHostSupported && directHostCallEnabled) {
            schedulerApi = unwrappedSchedulerApi;
        } else {
            schedulerApi =
                    new SchedulerApiWrapper(unwrappedSchedulerApi, threadUtils, mainThreadRunner);
        }

        // Build default component instances if necessary.
        if (protoExtensionProvider == null) {
            // Return an empty list of extensions by default.
            protoExtensionProvider = ArrayList::new;
        }
        FeedExtensionRegistry extensionRegistry = new FeedExtensionRegistry(protoExtensionProvider);
        if (clock == null) {
            clock = new SystemClockImpl();
        }
        TimingUtils timingUtils = new TimingUtils();
        TaskQueue taskQueue =
                new TaskQueue(basicLoggingApi, sequencedExecutor, mainThreadRunner, clock);
        FeedStore store = new FeedStore(configuration, timingUtils, extensionRegistry,
                contentStorage, journalStorage, threadUtils, taskQueue, clock, basicLoggingApi,
                mainThreadRunner);

        FeedAppLifecycleListener lifecycleListener = new FeedAppLifecycleListener(threadUtils);
        lifecycleListener.registerObserver(store);

        ProtocolAdapter protocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        ActionReader actionReader =
                new FeedActionReader(store, clock, protocolAdapter, taskQueue, configuration);
        FeedRequestManager feedRequestManager = new FeedRequestManagerImpl(configuration,
                networkClient, protocolAdapter, extensionRegistry, schedulerApi, taskQueue,
                timingUtils, threadUtils, actionReader, context, applicationInfo, mainThreadRunner,
                basicLoggingApi, tooltipSupportedApi);
        ActionUploadRequestManager actionUploadRequestManager =
                new FeedActionUploadRequestManager(configuration, networkClient, protocolAdapter,
                        extensionRegistry, taskQueue, threadUtils, store, clock);
        FeedSessionManagerFactory fsmFactory = new FeedSessionManagerFactory(taskQueue, store,
                timingUtils, threadUtils, protocolAdapter, feedRequestManager,
                actionUploadRequestManager, schedulerApi, configuration, clock, lifecycleListener,
                mainThreadRunner, basicLoggingApi);
        FeedSessionManager feedSessionManager = fsmFactory.create();
        RequestManagerImpl clientRequestManager =
                new RequestManagerImpl(feedRequestManager, feedSessionManager);
        ActionManager actionManager = new FeedActionManagerImpl(
                feedSessionManager, store, threadUtils, taskQueue, mainThreadRunner, clock);

        FeedKnownContent feedKnownContent =
                new FeedKnownContentImpl(feedSessionManager, mainThreadRunner, threadUtils);

        ClearAllListener clearAllListener = new ClearAllListener(
                taskQueue, feedSessionManager, store, threadUtils, lifecycleListener);
        return new FeedProcessScope(basicLoggingApi, networkClient,
                Validators.checkNotNull(protocolAdapter),
                Validators.checkNotNull(clientRequestManager),
                Validators.checkNotNull(feedSessionManager), store, timingUtils, threadUtils,
                taskQueue, mainThreadRunner, lifecycleListener, clock, debugBehavior, actionManager,
                configuration, feedKnownContent, extensionRegistry, clearAllListener,
                tooltipSupportedApi, applicationInfo);
    }
}
