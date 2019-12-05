// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;

import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.feedstore.testing.AbstractClearableFeedStoreTest;
import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingContentStorage;
import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingJournalStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryContentStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryJournalStorage;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Tests of the {@link
 * org.chromium.chrome.browser.feed.library.feedstore.internal.PersistentFeedStore} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PersistentFeedStoreTest extends AbstractClearableFeedStoreTest {
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry mExtensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);
    private final ContentStorageDirect mContentStorage = new InMemoryContentStorage();
    private final JournalStorageDirect mJournalStorage = new InMemoryJournalStorage();
    private final FakeBasicLoggingApi mBasicLoggingApi = new FakeBasicLoggingApi();
    private final FakeMainThreadRunner mMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    private FakeTaskQueue mFakeTaskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeTaskQueue.initialize(() -> {});
        mFakeThreadUtils.enforceMainThread(false);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new PersistentFeedStore(Configuration.getDefaultInstance(), mTimingUtils,
                mExtensionRegistry, mContentStorage, mJournalStorage, mFakeTaskQueue,
                mFakeThreadUtils, mFakeClock, new FeedStoreHelper(), mBasicLoggingApi,
                mainThreadRunner);
    }

    @Test
    public void clearStorage_contentStorage_failure_getAllContent() {
        ContentStorageDirect contentStorageSpy = spy(new DelegatingContentStorage(mContentStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                mTimingUtils, mExtensionRegistry, contentStorageSpy, mJournalStorage,
                mFakeTaskQueue, mFakeThreadUtils, mFakeClock, new FeedStoreHelper(),
                mBasicLoggingApi, mMainThreadRunner);
        doAnswer(ans -> Result.failure()).when(contentStorageSpy).getAllKeys();

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_contentStorage_failure_commit() {
        ContentStorageDirect contentStorageSpy = spy(new DelegatingContentStorage(mContentStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                mTimingUtils, mExtensionRegistry, contentStorageSpy, mJournalStorage,
                mFakeTaskQueue, mFakeThreadUtils, mFakeClock, new FeedStoreHelper(),
                mBasicLoggingApi, mMainThreadRunner);

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
        JournalStorageDirect journalStorageSpy = spy(new DelegatingJournalStorage(mJournalStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                mTimingUtils, mExtensionRegistry, mContentStorage, journalStorageSpy,
                mFakeTaskQueue, mFakeThreadUtils, mFakeClock, new FeedStoreHelper(),
                mBasicLoggingApi, mMainThreadRunner);

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
        JournalStorageDirect journalStorageSpy = spy(new DelegatingJournalStorage(mJournalStorage));
        PersistentFeedStore store = new PersistentFeedStore(Configuration.getDefaultInstance(),
                mTimingUtils, mExtensionRegistry, mContentStorage, journalStorageSpy,
                mFakeTaskQueue, mFakeThreadUtils, mFakeClock, new FeedStoreHelper(),
                mBasicLoggingApi, mMainThreadRunner);
        doAnswer(ans -> Result.failure()).when(journalStorageSpy).getAllJournals();

        boolean clearSuccess = store.clearNonActionContent();
        assertThat(clearSuccess).isFalse();
    }

    @Test
    public void clearStorage_allStorage() {
        PersistentFeedStore store = (PersistentFeedStore) getStore(mMainThreadRunner);

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
        PersistentFeedStore store = (PersistentFeedStore) getStore(mMainThreadRunner);

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
        PersistentFeedStore store = (PersistentFeedStore) getStore(mMainThreadRunner);

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
        PersistentFeedStore store = (PersistentFeedStore) getStore(mMainThreadRunner);

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
                getStore(mMainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
    }

    @Test
    public void getPayloads_withContent() {
        StreamPayload streamPayload = StreamPayload.newBuilder()
                                              .setStreamFeature(StreamFeature.getDefaultInstance())
                                              .build();
        mContentStorage.commit(
                new ContentMutation.Builder().upsert("foo", streamPayload.toByteArray()).build());

        Result<List<PayloadWithId>> result =
                getStore(mMainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).hasSize(1);
        assertThat(result.getValue().get(0).payload).isEqualTo(streamPayload);
    }

    @Test
    public void getPayloads_cannotParse() {
        mContentStorage.commit(new ContentMutation.Builder().upsert("foo", new byte[] {5}).build());

        Result<List<PayloadWithId>> result =
                getStore(mMainThreadRunner).getPayloads(ImmutableList.of("foo"));
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
        assertThat(mBasicLoggingApi.lastInternalError).isEqualTo(InternalFeedError.ITEM_NOT_PARSED);
    }

    @Test
    public void getSharedStates_noContent() {
        Result<List<StreamSharedState>> result = getStore(mMainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
    }

    @Test
    public void getSharedStates_withContent() {
        StreamSharedState sharedState = StreamSharedState.newBuilder().setContentId("foo").build();
        mContentStorage.commit(
                new ContentMutation.Builder()
                        .upsert(SHARED_STATE_PREFIX + "bar", sharedState.toByteArray())
                        .build());

        Result<List<StreamSharedState>> result = getStore(mMainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).containsExactly(sharedState);
    }

    @Test
    public void getSharedStates_cannotParse() {
        mContentStorage.commit(new ContentMutation.Builder()
                                       .upsert(SHARED_STATE_PREFIX + "bar", new byte[] {5})
                                       .build());

        Result<List<StreamSharedState>> result = getStore(mMainThreadRunner).getSharedStates();
        assertThat(result.isSuccessful()).isFalse();
    }
}
