// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore;

import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.DISMISS_ACTION_JOURNAL;
import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentOperation;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.Upsert;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation.Append;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.api.internal.store.StoreListener;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import com.google.android.libraries.feed.feedstore.testing.AbstractFeedStoreTest;
import com.google.android.libraries.feed.feedstore.testing.DelegatingContentStorage;
import com.google.android.libraries.feed.feedstore.testing.DelegatingJournalStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryContentStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryJournalStorage;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;

/** Tests of the {@link FeedStore} class. */
@RunWith(RobolectricTestRunner.class)
public class FeedStoreTest extends AbstractFeedStoreTest {
    private static final String CONTENT_ID = "contentId";
    private static final StreamStructure STREAM_STRUCTURE =
            StreamStructure.newBuilder()
                    .setContentId(CONTENT_ID)
                    .setOperation(Operation.UPDATE_OR_APPEND)
                    .build();
    private static final StreamPayload PAYLOAD =
            StreamPayload.newBuilder()
                    .setStreamFeature(StreamFeature.newBuilder().setContentId(CONTENT_ID))
                    .build();
    private static final byte[] SEMANTIC_PROPERTIES = new byte[] {4, 12, 18, 5};

    private final ContentStorageDirect contentStorage = new InMemoryContentStorage();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry extensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);

    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private Configuration configuration;
    @Mock
    private StoreListener listener;
    private FakeMainThreadRunner mainThreadRunner;
    private FakeTaskQueue taskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        when(configuration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false))
                .thenReturn(false);
        taskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        taskQueue.initialize(() -> {});
        mainThreadRunner = FakeMainThreadRunner.runTasksImmediately();
        fakeThreadUtils.enforceMainThread(false);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new FeedStore(configuration, timingUtils, extensionRegistry, contentStorage,
                new InMemoryJournalStorage(), fakeThreadUtils, taskQueue, fakeClock,
                basicLoggingApi, this.mainThreadRunner);
    }

    @Test
    public void testSwitchToEphemeralMode() {
        FeedStore store = (FeedStore) getStore(mainThreadRunner);
        assertThat(store.isEphemeralMode()).isFalse();
        store.switchToEphemeralMode();
        assertThat(store.isEphemeralMode()).isTrue();
        verify(basicLoggingApi).onInternalError(InternalFeedError.SWITCH_TO_EPHEMERAL);
    }

    @Test
    public void testSwitchToEphemeralMode_listeners() {
        FeedStore store = (FeedStore) getStore(mainThreadRunner);
        assertThat(store.isEphemeralMode()).isFalse();

        store.registerObserver(listener);

        store.switchToEphemeralMode();
        assertThat(store.isEphemeralMode()).isTrue();
        verify(listener).onSwitchToEphemeralMode();
    }

    @Test
    public void testDumpEphemeralActions_notEphemeralMode() {
        JournalStorageDirect journalStorageSpy =
                spy(new DelegatingJournalStorage(new InMemoryJournalStorage()));
        ContentStorageDirect contentStorageSpy =
                spy(new DelegatingContentStorage(this.contentStorage));
        FeedStore store = new FeedStore(configuration, timingUtils, extensionRegistry,
                contentStorageSpy, journalStorageSpy, fakeThreadUtils, taskQueue, fakeClock,
                basicLoggingApi, mainThreadRunner);
        store.onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
        verifyZeroInteractions(journalStorageSpy, contentStorageSpy);
    }

    @Test
    public void testDumpEphemeralActions_ephemeralMode() throws InvalidProtocolBufferException {
        JournalStorageDirect journalStorageSpy =
                spy(new DelegatingJournalStorage(new InMemoryJournalStorage()));
        ContentStorageDirect contentStorageSpy =
                spy(new DelegatingContentStorage(this.contentStorage));
        FeedStore store = new FeedStore(configuration, timingUtils, extensionRegistry,
                contentStorageSpy, journalStorageSpy, fakeThreadUtils, taskQueue, fakeClock,
                basicLoggingApi, mainThreadRunner);
        store.switchToEphemeralMode();
        reset(journalStorageSpy, contentStorageSpy);

        // Add ephemeral semantic properties, content, and actions
        store.editSemanticProperties()
                .add(CONTENT_ID, ByteString.copyFrom(SEMANTIC_PROPERTIES))
                .commit();
        store.editLocalActions().add(ActionType.DISMISS, CONTENT_ID).commit();
        store.editContent().add(CONTENT_ID, PAYLOAD).commit();
        store.editSession(Store.HEAD_SESSION_ID).add(STREAM_STRUCTURE).commit();

        store.onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);

        // Verify content is written for semantic properties and actions only
        ArgumentCaptor<JournalMutation> journalMutationArgumentCaptor =
                ArgumentCaptor.forClass(JournalMutation.class);
        verify(journalStorageSpy).commit(journalMutationArgumentCaptor.capture());

        JournalMutation journalMutation = journalMutationArgumentCaptor.getValue();
        assertThat(journalMutation.getJournalName()).isEqualTo(DISMISS_ACTION_JOURNAL);
        assertThat(journalMutation.getOperations()).hasSize(1);
        assertThat(journalMutation.getOperations().get(0).getType())
                .isEqualTo(JournalOperation.Type.APPEND);
        byte[] journalMutationBytes = ((Append) journalMutation.getOperations().get(0)).getValue();
        StreamLocalAction action = StreamLocalAction.parseFrom(journalMutationBytes);
        assertThat(action.getAction()).isEqualTo(ActionType.DISMISS);
        assertThat(action.getFeatureContentId()).isEqualTo(CONTENT_ID);

        ArgumentCaptor<ContentMutation> contentMutationArgumentCaptor =
                ArgumentCaptor.forClass(ContentMutation.class);
        verify(contentStorageSpy).commit(contentMutationArgumentCaptor.capture());

        ContentMutation contentMutation = contentMutationArgumentCaptor.getValue();
        assertThat(contentMutation.getOperations()).hasSize(1);
        assertThat(contentMutation.getOperations().get(0).getType())
                .isEqualTo(ContentOperation.Type.UPSERT);
        Upsert upsert = (Upsert) contentMutation.getOperations().get(0);
        assertThat(upsert.getKey()).isEqualTo(SEMANTIC_PROPERTIES_PREFIX + CONTENT_ID);
        assertThat(upsert.getValue()).isEqualTo(SEMANTIC_PROPERTIES);
    }
}
