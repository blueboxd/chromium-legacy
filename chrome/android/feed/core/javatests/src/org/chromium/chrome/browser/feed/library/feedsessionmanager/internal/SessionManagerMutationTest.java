// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.common.MutationContext.EMPTY_CONTEXT;

import android.util.Pair;

import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionManagerMutation.MutationCommitter;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

/**
 * Tests of the {@link SessionManagerMutation} and the actual committer {@link
 * SessionManagerMutation.MutationCommitter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionManagerMutationTest {
    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final FakeBasicLoggingApi mFakeBasicLoggingApi = new FakeBasicLoggingApi();
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeMainThreadRunner mFakeMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final String mRootContentId = mIdGenerators.createRootContentId(0);
    private final TimingUtils mTimingUtils = new TimingUtils();

    @Mock
    private KnownContent.Listener mKnownContentListener;
    @Mock
    private SchedulerApi mSchedulerApi;
    private ContentCache mContentCache;
    private FakeTaskQueue mFakeTaskQueue;
    private ModelError mNotifyError;
    private Session mNotifySession;
    private SessionCache mSessionCache;
    private FakeStore mFakeStore;

    @Before
    public void setUp() {
        initMocks(this);
        mNotifySession = null;
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeTaskQueue.initialize(() -> {});
        mFakeThreadUtils.enforceMainThread(false);
        mFakeStore = new FakeStore(mConfiguration, mFakeThreadUtils, mFakeTaskQueue, mFakeClock);
        SessionFactory sessionFactory = new SessionFactory(
                mFakeStore, mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mConfiguration);
        mSessionCache = new SessionCache(mFakeStore, mFakeTaskQueue, sessionFactory, 10L,
                mTimingUtils, mFakeThreadUtils, mFakeClock);
        mSessionCache.initialize();
        mContentCache = new ContentCache();
    }

    @Test
    public void testResultError() {
        String sessionId = "session:1";
        Session session = getSession(sessionId);

        MutationContext mutationContext =
                new MutationContext.Builder()
                        .setContinuationToken(StreamToken.getDefaultInstance())
                        .setRequestingSessionId(sessionId)
                        .build();
        MutationCommitter mutationCommitter = getMutationCommitter(mutationContext);
        mutationCommitter.accept(Result.failure());
        assertThat(mNotifySession).isEqualTo(session);
        assertThat(mNotifyError.getErrorType()).isEqualTo(ErrorType.PAGINATION_ERROR);
    }

    @Test
    public void testResultError_noSession() {
        MutationCommitter mutationCommitter = getMutationCommitter(MutationContext.EMPTY_CONTEXT);
        mutationCommitter.accept(Result.failure());
        assertThat(mNotifySession).isEqualTo(null);
        assertThat(mNotifyError.getErrorType()).isEqualTo(ErrorType.NO_CARDS_ERROR);
    }

    @Test
    public void testResetHead() {
        mFakeClock.set(5L);
        List<StreamDataOperation> dataOperations = new ArrayList<>();
        dataOperations.add(getStreamDataOperation(
                StreamStructure.newBuilder().setOperation(Operation.CLEAR_ALL).build(), null));
        String sessionId = "session:1";

        MutationCommitter mutationCommitter = getMutationCommitter(
                new MutationContext.Builder().setRequestingSessionId(sessionId).build());
        mutationCommitter.accept(Result.success(Model.of(dataOperations)));
        assertThat(mutationCommitter.mClearedHead).isTrue();
        verify(mSchedulerApi).onReceiveNewContent(5L);
        verify(mKnownContentListener).onNewContentReceived(true, 5L);
    }

    @Test
    public void testUpdateHeadMetadata() {
        int schemaVersion = 3;
        long currentTime = 8L;
        mFakeClock.set(currentTime);
        MutationCommitter mutationCommitter = getMutationCommitter(MutationContext.EMPTY_CONTEXT);
        mutationCommitter.accept(Result.success(Model.of(
                ImmutableList.of(getStreamDataOperation(
                        StreamStructure.newBuilder().setOperation(Operation.CLEAR_ALL).build(),
                        /* payload= */ null)),
                schemaVersion)));
        assertThat(mSessionCache.getHead().getSchemaVersion()).isEqualTo(schemaVersion);
        assertThat(mSessionCache.getHeadLastAddedTimeMillis()).isEqualTo(currentTime);
    }

    @Test
    public void testUpdateContent() {
        mFakeClock.set(8L);
        List<String> contentIds = getContentIds(3);
        List<Pair<StreamStructure, StreamPayload>> features = getFeatures(contentIds,
                ()
                        -> StreamPayload.newBuilder()
                                   .setStreamFeature(StreamFeature.getDefaultInstance())
                                   .build());
        List<StreamDataOperation> dataOperations = new ArrayList<>();
        for (Pair<StreamStructure, StreamPayload> feature : features) {
            dataOperations.add(getStreamDataOperation(feature.first, feature.second));
        }
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        mutationCommitter.accept(Result.success(Model.of(dataOperations)));

        Result<List<PayloadWithId>> result = mFakeStore.getPayloads(contentIds);
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).hasSize(contentIds.size());
        for (PayloadWithId payload : result.getValue()) {
            assertThat(contentIds).contains(payload.contentId);
        }
        assertThat(mContentCache.size()).isEqualTo(0);
        verify(mSchedulerApi, never()).onReceiveNewContent(anyLong());
        verify(mKnownContentListener).onNewContentReceived(false, 8L);
    }

    @Test
    public void testUpdateContent_failedCommitLogsError() {
        mFakeStore.setAllowEditContent(false);

        List<String> contentIds = getContentIds(3);
        List<Pair<StreamStructure, StreamPayload>> features = getFeatures(contentIds,
                ()
                        -> StreamPayload.newBuilder()
                                   .setStreamFeature(StreamFeature.getDefaultInstance())
                                   .build());
        List<StreamDataOperation> dataOperations = new ArrayList<>();
        for (Pair<StreamStructure, StreamPayload> feature : features) {
            dataOperations.add(getStreamDataOperation(feature.first, feature.second));
        }
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        mutationCommitter.accept(Result.success(Model.of(dataOperations)));

        assertThat(mFakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.CONTENT_MUTATION_FAILED);
    }

    @Test
    public void testSemanticData() {
        List<String> contentIds = getContentIds(2);
        List<Pair<StreamStructure, StreamPayload>> features = getFeatures(contentIds,
                ()
                        -> StreamPayload.newBuilder()
                                   .setSemanticData(
                                           ByteString.copyFrom("foo", Charset.defaultCharset()))
                                   .build());
        List<StreamDataOperation> dataOperations = new ArrayList<>();
        for (Pair<StreamStructure, StreamPayload> feature : features) {
            dataOperations.add(getStreamDataOperation(feature.first, feature.second));
        }
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        mutationCommitter.accept(Result.success(Model.of(dataOperations)));

        Result<List<SemanticPropertiesWithId>> result =
                mFakeStore.getSemanticProperties(contentIds);
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).hasSize(contentIds.size());
        for (SemanticPropertiesWithId payload : result.getValue()) {
            assertThat(contentIds).contains(payload.contentId);
        }
    }

    @Test
    public void testValidDataOperation() {
        StreamDataOperation operation = StreamDataOperation.getDefaultInstance();
        assertThat(SessionManagerMutation.validDataOperation(operation)).isFalse();

        operation = StreamDataOperation.newBuilder()
                            .setStreamPayload(StreamPayload.getDefaultInstance())
                            .build();
        assertThat(SessionManagerMutation.validDataOperation(operation)).isFalse();

        operation = StreamDataOperation.newBuilder()
                            .setStreamStructure(StreamStructure.getDefaultInstance())
                            .build();
        assertThat(SessionManagerMutation.validDataOperation(operation)).isFalse();

        operation = StreamDataOperation.newBuilder()
                            .setStreamPayload(StreamPayload.getDefaultInstance())
                            .setStreamStructure(StreamStructure.getDefaultInstance())
                            .build();
        assertThat(SessionManagerMutation.validDataOperation(operation)).isFalse();

        operation = StreamDataOperation.newBuilder()
                            .setStreamPayload(StreamPayload.getDefaultInstance())
                            .setStreamStructure(
                                    StreamStructure.newBuilder().setContentId("content").build())
                            .build();
        assertThat(SessionManagerMutation.validDataOperation(operation)).isTrue();
    }

    @Test
    public void testInvalidateHead() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        mutationCommitter.resetHead(null);
        assertThat(mFakeStore.getClearHeadCalled()).isTrue();
    }

    @Test
    public void testShouldInvalidateSession_modelProviderInitializing() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.INITIALIZING);
        assertThat(mutationCommitter.shouldInvalidateSession(null, modelProvider)).isFalse();
    }

    @Test
    public void testShouldInvalidateSession_modelProviderInvalidated() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.INVALIDATED);
        assertThat(mutationCommitter.shouldInvalidateSession(null, modelProvider)).isFalse();
    }

    @Test
    public void testShouldInvalidateSession_modelProviderReady() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.READY);
        assertThat(mutationCommitter.shouldInvalidateSession(null, modelProvider)).isTrue();
    }

    @Test
    public void testShouldInvalidateSession_noModelProviderSession() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.READY);
        assertThat(mutationCommitter.shouldInvalidateSession("session:2", modelProvider)).isTrue();
    }

    @Test
    public void testShouldInvalidateSession_differentSession() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        String sessionId = "session:1";
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.READY);
        when(modelProvider.getSessionId()).thenReturn(sessionId);
        assertThat(mutationCommitter.shouldInvalidateSession("session:2", modelProvider)).isFalse();
    }

    @Test
    public void testShouldInvalidateSession_sameSession() {
        MutationCommitter mutationCommitter = getMutationCommitter(EMPTY_CONTEXT);
        String sessionId = "session:1";
        ModelProvider modelProvider = mock(ModelProvider.class);
        when(modelProvider.getCurrentState()).thenReturn(State.READY);
        when(modelProvider.getSessionId()).thenReturn(sessionId);
        assertThat(mutationCommitter.shouldInvalidateSession(sessionId, modelProvider)).isTrue();
    }

    private MutationCommitter getMutationCommitter(MutationContext mutationContext) {
        SessionManagerMutation mutation = new SessionManagerMutation(mFakeStore, mSessionCache,
                mContentCache, mFakeTaskQueue, mSchedulerApi, mFakeThreadUtils, mTimingUtils,
                mFakeClock, mFakeMainThreadRunner, mFakeBasicLoggingApi);
        return (MutationCommitter) mutation.createCommitter(
                "task", mutationContext, this::notifySessionError, mKnownContentListener);
    }

    private List<String> getContentIds(int count) {
        List<String> contentIds = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            contentIds.add(mIdGenerators.createFeatureContentId(i));
        }
        return contentIds;
    }

    private Session getSession(String sessionId) {
        Session session = mock(Session.class);
        when(session.getSessionId()).thenReturn(sessionId);
        mSessionCache.putAttachedAndRetainMetadata(sessionId, session);
        return session;
    }

    private List<Pair<StreamStructure, StreamPayload>> getFeatures(
            List<String> contentIds, Supplier<StreamPayload> payloadConsumer) {
        List<Pair<StreamStructure, StreamPayload>> values = new ArrayList<>();
        for (String contentId : contentIds) {
            StreamStructure streamStructure = StreamStructure.newBuilder()
                                                      .setOperation(Operation.UPDATE_OR_APPEND)
                                                      .setParentContentId(mRootContentId)
                                                      .setContentId(contentId)
                                                      .build();
            StreamPayload streamPayload = payloadConsumer.get();
            values.add(new Pair<>(streamStructure, streamPayload));
        }
        return values;
    }

    private StreamDataOperation getStreamDataOperation(
            StreamStructure streamStructure, StreamPayload payload) {
        StreamDataOperation.Builder builder =
                StreamDataOperation.newBuilder().setStreamStructure(streamStructure);
        if (payload != null) {
            builder.setStreamPayload(payload);
        }
        return builder.build();
    }

    private void notifySessionError(Session session, ModelError error) {
        mNotifySession = session;
        mNotifyError = error;
    }
}
