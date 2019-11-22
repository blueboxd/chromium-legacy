// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.feedstore.testing.AbstractClearableFeedStoreTest;
import com.google.android.libraries.feed.feedstore.testing.DelegatingContentStorage;
import com.google.android.libraries.feed.feedstore.testing.DelegatingJournalStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryContentStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryJournalStorage;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Tests of the {@link com.google.android.libraries.feed.feedstore.internal.PersistentFeedStore}
 * class.
 */
@RunWith(RobolectricTestRunner.class)
public class PersistentFeedStoreTest extends AbstractClearableFeedStoreTest {
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry extensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);
    private final ContentStorageDirect contentStorage = new InMemoryContentStorage();
    private final JournalStorageDirect journalStorage = new InMemoryJournalStorage();
    private final FakeBasicLoggingApi basicLoggingApi = new FakeBasicLoggingApi();
    private final FakeMainThreadRunner mainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    private FakeTaskQueue fakeTaskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        fakeThreadUtils.enforceMainThread(false);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new PersistentFeedStore(Configuration.getDefaultInstance(), timingUtils,
                extensionRegistry, contentStorage, journalStorage, fakeTaskQueue, fakeThreadUtils,
                fakeClock, new FeedStoreHelper(), basicLoggingApi, mainThreadRunner);
    }

    @Test
    public void clearStorage_contentStorage_failure_getAllContent() {
        ContentStorageDirect contentStorageSpy = spy(new DelegatingContentStorage(contentStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                timingUtils, extensionRegistry, contentStorageSpy, journalStorage, fakeTaskQueue,
                fakeThreadUtils, fakeClock, new FeedStoreHelper(), basicLoggingApi,
                mainThreadRunner);
        doAnswer(ans -> Result.failure()).when(contentStorageSpy).getAllKeys();

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_contentStorage_failure_commit() {
        ContentStorageDirect contentStorageSpy = spy(new DelegatingContentStorage(contentStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                timingUtils, extensionRegistry, contentStorageSpy, journalStorage, fakeTaskQueue,
                fakeThreadUtils, fakeClock, new FeedStoreHelper(), basicLoggingApi,
                mainThreadRunner);

        CommitResult commitResult = store.editContent().add(CONTENT_ID, STREAM_PAYLOAD).commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<PayloadWithId>> payloadsResult =
                store.getPayloads(Collections.singletonList(CONTENT_ID));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(1);
        assertThat(payloadsResult.getValue().get(0).contentId).isEqualTo(CONTENT_ID);
        assertThat(payloadsResult.getValue().get(0).payload).isEqualTo(STREAM_PAYLOAD);
        doAnswer(ans -> CommitResult.FAILURE)
                .when(contentStorageSpy)
                .commit(any(ContentMutation.class));

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_journalStorage_failure_getAllJournals() {
        JournalStorageDirect journalStorageSpy = spy(new DelegatingJournalStorage(journalStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                timingUtils, extensionRegistry, contentStorage, journalStorageSpy, fakeTaskQueue,
                fakeThreadUtils, fakeClock, new FeedStoreHelper(), basicLoggingApi,
                mainThreadRunner);

        boolean commitResult =
                store.editSession(SESSION_ID)
                        .add(StreamStructure.newBuilder().setContentId(CONTENT_ID).build())
                        .commit();
        assertThat(commitResult).isTrue();

        Result<List<String>> sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        assertThat(sessionsResult.getValue()).hasSize(1);
        assertThat(sessionsResult.getValue().get(0)).isEqualTo(SESSION_ID);

        doAnswer(ans -> CommitResult.FAILURE)
                .when(journalStorageSpy)
                .commit(any(JournalMutation.class));

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_journalStorage_failure_deleteJournal() {
        JournalStorageDirect journalStorageSpy = spy(new DelegatingJournalStorage(journalStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                timingUtils, extensionRegistry, contentStorage, journalStorageSpy, fakeTaskQueue,
                fakeThreadUtils, fakeClock, new FeedStoreHelper(), basicLoggingApi,
                mainThreadRunner);
        doAnswer(ans -> Result.failure()).when(journalStorageSpy).getAllJournals();

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_allStorage() {
        PersistentFeedStore store = (PersistentFeedStore) getStore(mainThreadRunner);

        /*
        SETUP
        */

        // Payload
        CommitResult commitResult = store.editContent()
                                            .add(CONTENT_ID, STREAM_PAYLOAD)
                                            .add(CONTENT_ID_2, STREAM_PAYLOAD_2)
                                            .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<PayloadWithId>> payloadsResult =
                store.getPayloads(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(2);

        // Semantic properties
        commitResult = store.editSemanticProperties()
                               .add(CONTENT_ID, ByteString.copyFrom(SEMANTIC_PROPERTIES))
                               .add(CONTENT_ID_2, ByteString.copyFrom(SEMANTIC_PROPERTIES_2))
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                store.getSemanticProperties(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        assertThat(semanticPropertiesResult.getValue()).hasSize(2);

        // Shared State
        commitResult = store.editContent()
                               .add(CONTENT_ID, STREAM_PAYLOAD_SHARED_STATE)
                               .add(CONTENT_ID_2, STREAM_PAYLOAD_SHARED_STATE_2)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<StreamSharedState>> sharedStatesResult = store.getSharedStates();
        assertThat(sharedStatesResult.isSuccessful()).isTrue();
        assertThat(sharedStatesResult.getValue()).hasSize(2);

        // Journal
        boolean boolCommitResult =
                store.editSession(SESSION_ID)
                        .add(StreamStructure.newBuilder().setContentId(CONTENT_ID).build())
                        .commit();
        assertThat(boolCommitResult).isTrue();
        boolCommitResult =
                store.editSession(SESSION_ID_2)
                        .add(StreamStructure.newBuilder().setContentId(CONTENT_ID_2).build())
                        .commit();
        assertThat(boolCommitResult).isTrue();

        Result<List<String>> sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        assertThat(sessionsResult.getValue()).hasSize(2);

        // Actions
        commitResult = store.editLocalActions()
                               .add(ActionType.DISMISS, CONTENT_ID)
                               .add(ActionType.DISMISS, CONTENT_ID_2)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<StreamLocalAction>> dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(2);

        commitResult = store.editUploadableActions()
                               .upsert(StreamUploadableAction.newBuilder()
                                               .setFeatureContentId(CONTENT_ID)
                                               .build(),
                                       CONTENT_ID)
                               .upsert(StreamUploadableAction.newBuilder()
                                               .setFeatureContentId(CONTENT_ID_2)
                                               .build(),
                                       CONTENT_ID_2)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<Set<StreamUploadableAction>> uploadableActionsResult =
                store.getAllUploadableActions();
        assertThat(uploadableActionsResult.isSuccessful()).isTrue();
        assertThat(uploadableActionsResult.getValue()).hasSize(2);

        /*
        CLEAR
        */

        assertThat(store.clearNonActionContent()).isTrue();

        /*
        VERIFICATION
        */

        // Payload
        payloadsResult = store.getPayloads(Collections.singletonList(CONTENT_ID));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(0);

        // Semantic properties (should not be cleared)
        semanticPropertiesResult =
                store.getSemanticProperties(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        assertThat(semanticPropertiesResult.getValue()).hasSize(2);
        assertThat(semanticPropertiesResult.getValue())
                .containsExactly(new SemanticPropertiesWithId(CONTENT_ID, SEMANTIC_PROPERTIES),
                        new SemanticPropertiesWithId(CONTENT_ID_2, SEMANTIC_PROPERTIES_2));

        // Shared state
        sharedStatesResult = store.getSharedStates();
        assertThat(sharedStatesResult.isSuccessful()).isTrue();
        assertThat(sharedStatesResult.getValue()).hasSize(0);

        // Journal
        sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        assertThat(sessionsResult.getValue()).hasSize(0);

        // Actions (should not be cleared)
        dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(2);
        assertThat(dismissActionsResult.getValue().get(0).getFeatureContentId())
                .isEqualTo(CONTENT_ID);
        assertThat(dismissActionsResult.getValue().get(0).getAction())
                .isEqualTo(ActionType.DISMISS);
        assertThat(dismissActionsResult.getValue().get(1).getFeatureContentId())
                .isEqualTo(CONTENT_ID_2);
        assertThat(dismissActionsResult.getValue().get(1).getAction())
                .isEqualTo(ActionType.DISMISS);

        // UploadableActions (should be cleared)
        uploadableActionsResult = store.getAllUploadableActions();
        assertThat(uploadableActionsResult.isSuccessful()).isTrue();
        assertThat(uploadableActionsResult.getValue()).isEmpty();
    }

    @Test
    public void uploadActions_removedBeforeUpserted_stillUpserts() {
        PersistentFeedStore store = (PersistentFeedStore) getStore(mainThreadRunner);

        CommitResult commitResult = store.editUploadableActions()
                                            .upsert(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID)
                                                            .build(),
                                                    CONTENT_ID)
                                            .upsert(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID_2)
                                                            .build(),
                                                    CONTENT_ID_2)
                                            .remove(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID_2)
                                                            .build(),
                                                    CONTENT_ID_2)
                                            .upsert(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID_2)
                                                            .build(),
                                                    CONTENT_ID_2)
                                            .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<Set<StreamUploadableAction>> uploadableActionsResult =
                store.getAllUploadableActions();
        assertThat(uploadableActionsResult.isSuccessful()).isTrue();
        assertThat(uploadableActionsResult.getValue()).hasSize(2);
        assertThat(uploadableActionsResult.getValue())
                .containsAtLeast(
                        StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                        StreamUploadableAction.newBuilder()
                                .setFeatureContentId(CONTENT_ID_2)
                                .build());
    }

    @Test
    public void uploadActions_removedAfterCommittedUpsert_stillRemoves() {
        PersistentFeedStore store = (PersistentFeedStore) getStore(mainThreadRunner);

        CommitResult commitResult = store.editUploadableActions()
                                            .upsert(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID)
                                                            .build(),
                                                    CONTENT_ID)
                                            .commit();

        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);
        Result<Set<StreamUploadableAction>> uploadableActionsResult =
                store.getAllUploadableActions();
        assertThat(uploadableActionsResult.getValue()).hasSize(1);
        assertThat(uploadableActionsResult.getValue())
                .contains(StreamUploadableAction.newBuilder()
                                  .setFeatureContentId(CONTENT_ID)
                                  .build());

        commitResult = store.editUploadableActions()
                               .remove(StreamUploadableAction.newBuilder()
                                               .setFeatureContentId(CONTENT_ID)
                                               .build(),
                                       CONTENT_ID)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        uploadableActionsResult = store.getAllUploadableActions();
        assertThat(uploadableActionsResult.isSuccessful()).isTrue();
        assertThat(uploadableActionsResult.getValue()).isEmpty();
    }

    @Test
    public void uploadActions_removedNonExistantAction_succeeds() {
        PersistentFeedStore store = (PersistentFeedStore) getStore(mainThreadRunner);

        CommitResult commitResult = store.editUploadableActions()
                                            .remove(StreamUploadableAction.newBuilder()
                                                            .setFeatureContentId(CONTENT_ID)
                                                            .build(),
                                                    CONTENT_ID)
                                            .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<Set<StreamUploadableAction>> uploadableActionsResult =
                store.getAllUploadableActions();
        assertThat(uploadableActionsResult.isSuccessful()).isTrue();
        assertThat(uploadableActionsResult.getValue()).isEmpty();
    }

    @Test
    public void getPayloads_noContent() {
        Result<List<PayloadWithId>> result =
                getStore(mainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
    }

    @Test
    public void getPayloads_withContent() {
        StreamPayload streamPayload = StreamPayload.newBuilder()
                                              .setStreamFeature(StreamFeature.getDefaultInstance())
                                              .build();
        contentStorage.commit(
                new ContentMutation.Builder().upsert("foo", streamPayload.toByteArray()).build());

        Result<List<PayloadWithId>> result =
                getStore(mainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).hasSize(1);
        assertThat(result.getValue().get(0).payload).isEqualTo(streamPayload);
    }

    @Test
    public void getPayloads_cannotParse() {
        contentStorage.commit(new ContentMutation.Builder().upsert("foo", new byte[] {5}).build());

        Result<List<PayloadWithId>> result =
                getStore(mainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
        assertThat(basicLoggingApi.lastInternalError).isEqualTo(InternalFeedError.ITEM_NOT_PARSED);
    }

    @Test
    public void getSharedStates_noContent() {
        Result<List<StreamSharedState>> result = getStore(mainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
    }

    @Test
    public void getSharedStates_withContent() {
        StreamSharedState sharedState = StreamSharedState.newBuilder().setContentId("foo").build();
        contentStorage.commit(
                new ContentMutation.Builder()
                        .upsert(SHARED_STATE_PREFIX + "bar", sharedState.toByteArray())
                        .build());

        Result<List<StreamSharedState>> result = getStore(mainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).containsExactly(sharedState);
    }

    @Test
    public void getSharedStates_cannotParse() {
        contentStorage.commit(new ContentMutation.Builder()
                                      .upsert(SHARED_STATE_PREFIX + "bar", new byte[] {5})
                                      .build());

        Result<List<StreamSharedState>> result = getStore(mainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isFalse();
    }
}
