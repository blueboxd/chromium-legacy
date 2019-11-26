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

    private final Configuration mConfiguration;
    private final ContentStorageDirect mContentStorage;
    private final FakeClock mFakeClock;
    private final FakeDirectExecutor mFakeDirectExecutor;
    private final FakeMainThreadRunner mFakeMainThreadRunner;
    private final FakeFeedRequestManager mFakeFeedRequestManager;
    private final FakeThreadUtils mFakeThreadUtils;
    private final FeedAppLifecycleListener mAppLifecycleListener;
    private final FeedModelProviderFactory mModelProviderFactory;
    private final FeedProtocolAdapter mFeedProtocolAdapter;
    private final FeedSessionManagerImpl mFeedSessionManager;
    private final FeedStore mStore;
    private final JournalStorageDirect mJournalStorage;
    private final SchedulerApi mSchedulerApi;
    private final TaskQueue mTaskQueue;
    private final RequestManager mRequestManager;

    private InfraIntegrationScope(FakeThreadUtils fakeThreadUtils,
            FakeDirectExecutor fakeDirectExecutor, SchedulerApi schedulerApi, FakeClock fakeClock,
            Configuration configuration, ContentStorageDirect contentStorage,
            JournalStorageDirect journalStorage, FakeMainThreadRunner fakeMainThreadRunner) {
        this.mFakeClock = fakeClock;
        this.mConfiguration = configuration;
        this.mContentStorage = contentStorage;
        this.mJournalStorage = journalStorage;
        this.mFakeDirectExecutor = fakeDirectExecutor;
        this.mFakeMainThreadRunner = fakeMainThreadRunner;
        this.mSchedulerApi = schedulerApi;
        this.mFakeThreadUtils = fakeThreadUtils;
        TimingUtils timingUtils = new TimingUtils();
        mAppLifecycleListener = new FeedAppLifecycleListener(fakeThreadUtils);
        FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();

        FeedExtensionRegistry extensionRegistry =
                new FeedExtensionRegistry(new ExtensionProvider());
        mTaskQueue = new TaskQueue(
                fakeBasicLoggingApi, fakeDirectExecutor, fakeMainThreadRunner, fakeClock);
        mStore = new FeedStore(configuration, timingUtils, extensionRegistry, contentStorage,
                journalStorage, fakeThreadUtils, mTaskQueue, fakeClock, fakeBasicLoggingApi,
                fakeMainThreadRunner);
        mFeedProtocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        mFakeFeedRequestManager = new FakeFeedRequestManager(
                fakeThreadUtils, fakeMainThreadRunner, mFeedProtocolAdapter, mTaskQueue);
        FakeActionUploadRequestManager fakeActionUploadRequestManager =
                new FakeActionUploadRequestManager(FakeThreadUtils.withThreadChecks());
        mFeedSessionManager = new FeedSessionManagerFactory(mTaskQueue, mStore, timingUtils,
                fakeThreadUtils, mFeedProtocolAdapter, mFakeFeedRequestManager,
                fakeActionUploadRequestManager, schedulerApi, configuration, fakeClock,
                mAppLifecycleListener, fakeMainThreadRunner, fakeBasicLoggingApi)
                                      .create();
        new ClearAllListener(
                mTaskQueue, mFeedSessionManager, mStore, fakeThreadUtils, mAppLifecycleListener);
        mFeedSessionManager.initialize();
        mModelProviderFactory = new FeedModelProviderFactory(mFeedSessionManager, fakeThreadUtils,
                timingUtils, mTaskQueue, fakeMainThreadRunner, configuration, fakeBasicLoggingApi);
        mRequestManager = new RequestManagerImpl(mFakeFeedRequestManager, mFeedSessionManager);
    }

    public ProtocolAdapter getProtocolAdapter() {
        return mFeedProtocolAdapter;
    }

    public FeedSessionManager getFeedSessionManager() {
        return mFeedSessionManager;
    }

    public ModelProviderFactory getModelProviderFactory() {
        return mModelProviderFactory;
    }

    public FakeClock getFakeClock() {
        return mFakeClock;
    }

    public FakeDirectExecutor getFakeDirectExecutor() {
        return mFakeDirectExecutor;
    }

    public FakeMainThreadRunner getFakeMainThreadRunner() {
        return mFakeMainThreadRunner;
    }

    public FakeThreadUtils getFakeThreadUtils() {
        return mFakeThreadUtils;
    }

    public FeedStore getStore() {
        return mStore;
    }

    public TaskQueue getTaskQueue() {
        return mTaskQueue;
    }

    public FakeFeedRequestManager getFakeFeedRequestManager() {
        return mFakeFeedRequestManager;
    }

    public AppLifecycleListener getAppLifecycleListener() {
        return mAppLifecycleListener;
    }

    public RequestManager getRequestManager() {
        return mRequestManager;
    }

    @Override
    public InfraIntegrationScope clone() {
        return new InfraIntegrationScope(mFakeThreadUtils, mFakeDirectExecutor, mSchedulerApi,
                mFakeClock, mConfiguration, mContentStorage, mJournalStorage,
                mFakeMainThreadRunner);
    }

    private static class ExtensionProvider implements ProtoExtensionProvider {
        @Override
        public List<GeneratedExtension<?, ?>> getProtoExtensions() {
            return new ArrayList<>();
        }
    }

    /** Builder for creating the {@link InfraIntegrationScope} */
    public static class Builder {
        private final FakeClock mFakeClock = new FakeClock();
        private final FakeMainThreadRunner mFakeMainThreadRunner =
                FakeMainThreadRunner.create(mFakeClock);
        private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

        private Configuration mConfiguration = Configuration.getDefaultInstance();
        private FakeDirectExecutor mFakeDirectExecutor =
                FakeDirectExecutor.runTasksImmediately(mFakeThreadUtils);
        private SchedulerApi mSchedulerApi1 = new FakeSchedulerApi(mFakeThreadUtils);

        public Builder() {}

        public Builder setConfiguration(Configuration configuration) {
            this.mConfiguration = configuration;
            return this;
        }

        public Builder setSchedulerApi(SchedulerApi schedulerApi) {
            this.mSchedulerApi1 = schedulerApi;
            return this;
        }

        public Builder withQueuingTasks() {
            mFakeDirectExecutor = FakeDirectExecutor.queueAllTasks(mFakeThreadUtils);
            return this;
        }

        public Builder withTimeoutSessionConfiguration(long timeoutMs) {
            mConfiguration = mConfiguration.toBuilder()
                                     .put(ConfigKey.USE_TIMEOUT_SCHEDULER, USE_TIMEOUT_SCHEDULER)
                                     .put(ConfigKey.TIMEOUT_TIMEOUT_MS, timeoutMs)
                                     .build();
            return this;
        }

        public InfraIntegrationScope build() {
            return new InfraIntegrationScope(mFakeThreadUtils, mFakeDirectExecutor, mSchedulerApi1,
                    mFakeClock, mConfiguration, new InMemoryContentStorage(),
                    new InMemoryJournalStorage(), mFakeMainThreadRunner);
        }
    }
}
