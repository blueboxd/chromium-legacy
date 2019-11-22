// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.feedactionmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.time.Duration;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests of the {@link FeedActionManagerImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class FeedActionManagerImplTest {
    private static final String CONTENT_ID_STRING = "contentIdString";
    private static final String SESSION_ID = "session";
    private static final long DEFAULT_TIME = Duration.ofDays(42).toMillis();
    private static final long DEFAULT_TIME_SECONDS = Duration.ofDays(42).getSeconds();

    private final FakeClock fakeClock = new FakeClock();
    private final FakeMainThreadRunner fakeMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Mock
    private FeedSessionManager feedSessionManager;
    @Mock
    private Store store;
    @Mock
    private LocalActionMutation localActionMutation;
    @Mock
    private Consumer<Result<Model>> modelConsumer;
    @Captor
    private ArgumentCaptor<Integer> actionTypeCaptor;
    @Captor
    private ArgumentCaptor<String> contentIdStringCaptor;
    @Captor
    private ArgumentCaptor<Result<Model>> modelCaptor;
    @Captor
    private ArgumentCaptor<MutationContext> mutationContextCaptor;
    @Captor
    private ArgumentCaptor<Consumer<Result<ConsistencyToken>>> consumerCaptor;
    @Captor
    private ArgumentCaptor<Set<StreamUploadableAction>> actionCaptor;

    private ActionManager actionManager;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        actionManager = new FeedActionManagerImpl(feedSessionManager, store, fakeThreadUtils,
                getTaskQueue(), fakeMainThreadRunner, fakeClock);
    }

    @Test
    public void dismissLocal() throws Exception {
        setUpDismissMocks();
        StreamDataOperation dataOperation = buildBasicDismissOperation();

        actionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), null);

        verify(localActionMutation)
                .add(actionTypeCaptor.capture(), contentIdStringCaptor.capture());
        assertThat(actionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(contentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(localActionMutation).commit();

        verify(modelConsumer).accept(modelCaptor.capture());
        Result<Model> result = modelCaptor.getValue();
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

        actionManager.dismissLocal(Collections.singletonList(CONTENT_ID_STRING),
                Collections.singletonList(dataOperation), SESSION_ID);

        verify(localActionMutation)
                .add(actionTypeCaptor.capture(), contentIdStringCaptor.capture());
        assertThat(actionTypeCaptor.getValue()).isEqualTo(ActionType.DISMISS);
        assertThat(contentIdStringCaptor.getValue()).isEqualTo(CONTENT_ID_STRING);

        verify(localActionMutation).commit();

        verify(feedSessionManager).getUpdateConsumer(mutationContextCaptor.capture());
        assertThat(mutationContextCaptor.getValue().getRequestingSessionId()).isEqualTo(SESSION_ID);

        verify(modelConsumer).accept(modelCaptor.capture());
        Result<Model> result = modelCaptor.getValue();
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

        actionManager.dismiss(Collections.singletonList(dataOperation), null);

        verify(modelConsumer).accept(modelCaptor.capture());
        Result<Model> result = modelCaptor.getValue();
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

        actionManager.dismiss(Collections.singletonList(dataOperation), SESSION_ID);

        verify(feedSessionManager).getUpdateConsumer(mutationContextCaptor.capture());
        assertThat(mutationContextCaptor.getValue().getRequestingSessionId()).isEqualTo(SESSION_ID);

        verify(modelConsumer).accept(modelCaptor.capture());
        Result<Model> result = modelCaptor.getValue();
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> streamDataOperations = result.getValue().streamDataOperations;
        assertThat(streamDataOperations).hasSize(1);
        StreamDataOperation streamDataOperation = streamDataOperations.get(0);
        assertThat(streamDataOperation).isEqualTo(dataOperation);
    }

    @Test
    public void triggerCreateAndUploadAction() throws Exception {
        ActionPayload payload = ActionPayload.getDefaultInstance();
        fakeClock.set(DEFAULT_TIME);
        actionManager.createAndUploadAction(CONTENT_ID_STRING, payload);
        verify(feedSessionManager).triggerUploadActions(actionCaptor.capture());
        StreamUploadableAction action =
                (StreamUploadableAction) actionCaptor.getValue().toArray()[0];
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
        actionManager.uploadAllActionsAndUpdateUrl(url, param, consumer);
        verify(feedSessionManager).fetchActionsAndUpload(consumerCaptor.capture());
        consumerCaptor.getValue().accept(Result.success(token));
    }

    private StreamDataOperation buildBasicDismissOperation() {
        return StreamDataOperation.newBuilder()
                .setStreamStructure(StreamStructure.newBuilder()
                                            .setContentId(CONTENT_ID_STRING)
                                            .setOperation(Operation.REMOVE))
                .build();
    }

    private void setUpDismissMocks() {
        when(feedSessionManager.getUpdateConsumer(any(MutationContext.class)))
                .thenReturn(modelConsumer);
        when(localActionMutation.add(anyInt(), anyString())).thenReturn(localActionMutation);
        when(store.editLocalActions()).thenReturn(localActionMutation);
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        return fakeTaskQueue;
    }
}
