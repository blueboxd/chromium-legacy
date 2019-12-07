// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.feedactionmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.time.Duration;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests of the {@link FeedActionManagerImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedActionManagerImplTest {
    private static final String CONTENT_ID_STRING = "contentIdString";
    private static final String SESSION_ID = "session";
    private static final long DEFAULT_TIME = Duration.ofDays(42).toMillis();
    private static final long DEFAULT_TIME_SECONDS = Duration.ofDays(42).getSeconds();

    private final FakeClock mFakeClock = new FakeClock();
    private final FakeMainThreadRunner mFakeMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Store mStore;
    @Mock
    private LocalActionMutation mLocalActionMutation;
    @Mock
    private Consumer<Result<Model>> mModelConsumer;
    @Captor
    private ArgumentCaptor<Integer> mActionTypeCaptor;
    @Captor
    private ArgumentCaptor<String> mContentIdStringCaptor;
    @Captor
    private ArgumentCaptor<Result<Model>> mModelCaptor;
    @Captor
    private ArgumentCaptor<MutationContext> mMutationContextCaptor;
    @Captor
    private ArgumentCaptor<Consumer<Result<ConsistencyToken>>> mConsumerCaptor;
    @Captor
    private ArgumentCaptor<Set<StreamUploadableAction>> mActionCaptor;

    private ActionManager mActionManager;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mActionManager = new FeedActionManagerImpl(mFeedSessionManager, mStore, mFakeThreadUtils,
                getTaskQueue(), mFakeMainThreadRunner, mFakeClock);
    }

    @Test
    public void dismissLocal() throws Exception {
        setUpDismissMocks();
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), null);

        verify(mLocalActionMutation)
                .add(mActionTypeCaptor.capture(), mContentIdStringCaptor.capture());
        assertThat(mActionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(mContentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(mLocalActionMutation).commit();

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismissLocal_sessionIdSet() throws Exception {
        setUpDismissMocks();
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), SESSION_ID);

        verify(mLocalActionMutation)
                .add(mActionTypeCaptor.capture(), mContentIdStringCaptor.capture());
        assertThat(mActionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(mContentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(mLocalActionMutation).commit();

        verify(mFeedSessionManager).getUpdateConsumer(mMutationContextCaptor.capture());
        assertThat(mMutationContextCaptor.getValue().getRequestingSessionId())
                .isEqualTo(SESSION_ID);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismiss() throws Exception {
        setUpDismissMocks();
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismiss(Collections.singletonList(dataOperation), null);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void dismiss_sessionIdSet() throws Exception {
        setUpDismissMocks();
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        mActionManager.dismiss(Collections.singletonList(dataOperation), SESSION_ID);

        verify(mFeedSessionManager).getUpdateConsumer(mMutationContextCaptor.capture());
        assertThat(mMutationContextCaptor.getValue().getRequestingSessionId())
                .isEqualTo(SESSION_ID);

        verify(mModelConsumer).accept(mModelCaptor.capture());
        Result<Model> result = mModelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void triggerCreateAndUploadAction() throws Exception {
        ActionPayload payload = ActionPayload.getDefaultInstance();
        mFakeClock.set(DEFAULT_TIME);
        mActionManager.createAndUploadAction(CONTENT_ID_STRING, payload);
        verify(mFeedSessionManager).triggerUploadActions(mActionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) mActionCaptor.getValue().toArray()[0];
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID_STRING);
        assertThat(action.getTimestampSeconds()).isEqualTo(DEFAULT_TIME_SECONDS);
        assertThat(action.getPayload()).isEqualTo(payload);
    }

    @Test
    public void triggerUploadAllActions() throws Exception {
        String url = "url";
        String param = "param";
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0x2}))
                                         .build();
        String expectedUrl = FeedActionManagerImpl.updateParam(url, param, token.toByteArray());
        Consumer<String> consumer = result -> {
            assertThat(result).isEqualTo(expectedUrl);
        };
        mActionManager.uploadAllActionsAndUpdateUrl(url, param, consumer);
        verify(mFeedSessionManager).fetchActionsAndUpload(mConsumerCaptor.capture());
        mConsumerCaptor.getValue().accept(Result.success(token));
    }

    private StreamDataOperation buildBasicDismissOperation() {
        return StreamDataOperation.newBuilder()
                .setStreamStructure(StreamStructure.newBuilder()
                                            .setContentId(CONTENT_ID_STRING)
                                            .setOperation(Operation.REMOVE))
                .build();
    }

    private void setUpDismissMocks() {
        when(mFeedSessionManager.getUpdateConsumer(any(MutationContext.class)))
                .thenReturn(mModelConsumer);
        when(mLocalActionMutation.add(anyInt(), anyString())).thenReturn(mLocalActionMutation);
        when(mStore.editLocalActions()).thenReturn(mLocalActionMutation);
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        return fakeTaskQueue;
    }
}
