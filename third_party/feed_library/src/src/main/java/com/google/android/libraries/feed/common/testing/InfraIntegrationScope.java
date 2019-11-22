// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.proto.ProtoExtensionProvider;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.scope.ClearAllListener;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeDirectExecutor;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedAppLifecycleListener;
import com.google.android.libraries.feed.feedmodelprovider.FeedModelProviderFactory;
import com.google.android.libraries.feed.feedprotocoladapter.FeedProtocolAdapter;
import com.google.android.libraries.feed.feedrequestmanager.RequestManagerImpl;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerFactory;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerImpl;
import com.google.android.libraries.feed.feedstore.FeedStore;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryContentStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryJournalStorage;
import com.google.android.libraries.feed.sharedstream.piet.PietRequiredContentAdapter;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.android.libraries.feed.testing.host.scheduler.FakeSchedulerApi;
import com.google.android.libraries.feed.testing.requestmanager.FakeActionUploadRequestManager;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.protobuf.GeneratedMessageLite.GeneratedExtension;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * This is a Scope type object which is used in the Infrastructure Integration tests. It sets the
 * Feed objects from ProtocolAdapter through the FeedSessionManager.
 */
public class InfraIntegrationScope {
    private static final boolean USE_TIMEOUT_SCHEDULER = true;

    /**
     * For the TimeoutSession tests, this is how long we allow it to run before declaring a timeout
     * error.
     */
    public static final long TIMEOUT_TEST_TIMEOUT = TimeUnit.SECONDS.toMillis(20);

    private final Configuration configuration;
    private final ContentStorageDirect contentStorage;
    private final FakeClock fakeClock;
    private final FakeDirectExecutor fakeDirectExecutor;
    private final FakeMainThreadRunner fakeMainThreadRunner;
    private final FakeFeedRequestManager fakeFeedRequestManager;
    private final FakeThreadUtils fakeThreadUtils;
    private final FeedAppLifecycleListener appLifecycleListener;
    private final FeedModelProviderFactory modelProviderFactory;
    private final FeedProtocolAdapter feedProtocolAdapter;
    private final FeedSessionManagerImpl feedSessionManager;
    private final FeedStore store;
    private final JournalStorageDirect journalStorage;
    private final SchedulerApi schedulerApi;
    private final TaskQueue taskQueue;
    private final RequestManager requestManager;

    private InfraIntegrationScope(FakeThreadUtils fakeThreadUtils,
            FakeDirectExecutor fakeDirectExecutor, SchedulerApi schedulerApi, FakeClock fakeClock,
            Configuration configuration, ContentStorageDirect contentStorage,
            JournalStorageDirect journalStorage, FakeMainThreadRunner fakeMainThreadRunner) {
        this.fakeClock = fakeClock;
        this.configuration = configuration;
        this.contentStorage = contentStorage;
        this.journalStorage = journalStorage;
        this.fakeDirectExecutor = fakeDirectExecutor;
        this.fakeMainThreadRunner = fakeMainThreadRunner;
        this.schedulerApi = schedulerApi;
        this.fakeThreadUtils = fakeThreadUtils;
        TimingUtils timingUtils = new TimingUtils();
        appLifecycleListener = new FeedAppLifecycleListener(fakeThreadUtils);
        FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();

        FeedExtensionRegistry extensionRegistry =
                new FeedExtensionRegistry(new ExtensionProvider());
        taskQueue = new TaskQueue(
                fakeBasicLoggingApi, fakeDirectExecutor, fakeMainThreadRunner, fakeClock);
        store = new FeedStore(configuration, timingUtils, extensionRegistry, contentStorage,
                journalStorage, fakeThreadUtils, taskQueue, fakeClock, fakeBasicLoggingApi,
                fakeMainThreadRunner);
        feedProtocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        fakeFeedRequestManager = new FakeFeedRequestManager(
                fakeThreadUtils, fakeMainThreadRunner, feedProtocolAdapter, taskQueue);
        FakeActionUploadRequestManager fakeActionUploadRequestManager =
                new FakeActionUploadRequestManager(FakeThreadUtils.withThreadChecks());
        feedSessionManager = new FeedSessionManagerFactory(taskQueue, store, timingUtils,
                fakeThreadUtils, feedProtocolAdapter, fakeFeedRequestManager,
                fakeActionUploadRequestManager, schedulerApi, configuration, fakeClock,
                appLifecycleListener, fakeMainThreadRunner, fakeBasicLoggingApi)
                                     .create();
        new ClearAllListener(
                taskQueue, feedSessionManager, store, fakeThreadUtils, appLifecycleListener);
        feedSessionManager.initialize();
        modelProviderFactory = new FeedModelProviderFactory(feedSessionManager, fakeThreadUtils,
                timingUtils, taskQueue, fakeMainThreadRunner, configuration, fakeBasicLoggingApi);
        requestManager = new RequestManagerImpl(fakeFeedRequestManager, feedSessionManager);
    }

    public ProtocolAdapter getProtocolAdapter() {
        return feedProtocolAdapter;
    }

    public FeedSessionManager getFeedSessionManager() {
        return feedSessionManager;
    }

    public ModelProviderFactory getModelProviderFactory() {
        return modelProviderFactory;
    }

    public FakeClock getFakeClock() {
        return fakeClock;
    }

    public FakeDirectExecutor getFakeDirectExecutor() {
        return fakeDirectExecutor;
    }

    public FakeMainThreadRunner getFakeMainThreadRunner() {
        return fakeMainThreadRunner;
    }

    public FakeThreadUtils getFakeThreadUtils() {
        return fakeThreadUtils;
    }

    public FeedStore getStore() {
        return store;
    }

    public TaskQueue getTaskQueue() {
        return taskQueue;
    }

    public FakeFeedRequestManager getFakeFeedRequestManager() {
        return fakeFeedRequestManager;
    }

    public AppLifecycleListener getAppLifecycleListener() {
        return appLifecycleListener;
    }

    public RequestManager getRequestManager() {
        return requestManager;
    }

    @Override
    public InfraIntegrationScope clone() {
        return new InfraIntegrationScope(fakeThreadUtils, fakeDirectExecutor, schedulerApi,
                fakeClock, configuration, contentStorage, journalStorage, fakeMainThreadRunner);
    }

    private static class ExtensionProvider implements ProtoExtensionProvider {
        @Override
        public List<GeneratedExtension<?, ?>> getProtoExtensions() {
            return new ArrayList<>();
        }
    }

    /** Builder for creating the {@link InfraIntegrationScope} */
    public static class Builder {
        private final FakeClock fakeClock = new FakeClock();
        private final FakeMainThreadRunner fakeMainThreadRunner =
                FakeMainThreadRunner.create(fakeClock);
        private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();

        private Configuration configuration = Configuration.getDefaultInstance();
        private FakeDirectExecutor fakeDirectExecutor =
                FakeDirectExecutor.runTasksImmediately(fakeThreadUtils);
        private SchedulerApi schedulerApi = new FakeSchedulerApi(fakeThreadUtils);

        public Builder() {}

        public Builder setConfiguration(Configuration configuration) {
            this.configuration = configuration;
            return this;
        }

        public Builder setSchedulerApi(SchedulerApi schedulerApi) {
            this.schedulerApi = schedulerApi;
            return this;
        }

        public Builder withQueuingTasks() {
            fakeDirectExecutor = FakeDirectExecutor.queueAllTasks(fakeThreadUtils);
            return this;
        }

        public Builder withTimeoutSessionConfiguration(long timeoutMs) {
            configuration = configuration.toBuilder()
                                    .put(ConfigKey.USE_TIMEOUT_SCHEDULER, USE_TIMEOUT_SCHEDULER)
                                    .put(ConfigKey.TIMEOUT_TIMEOUT_MS, timeoutMs)
                                    .build();
            return this;
        }

        public InfraIntegrationScope build() {
            return new InfraIntegrationScope(fakeThreadUtils, fakeDirectExecutor, schedulerApi,
                    fakeClock, configuration, new InMemoryContentStorage(),
                    new InMemoryJournalStorage(), fakeMainThreadRunner);
        }
    }
}
