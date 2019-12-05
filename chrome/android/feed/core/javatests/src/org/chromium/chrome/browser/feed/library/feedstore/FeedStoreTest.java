// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.DISMISS_ACTION_JOURNAL;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Upsert;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Append;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import org.chromium.chrome.browser.feed.library.feedstore.testing.AbstractFeedStoreTest;
import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingContentStorage;
import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingJournalStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryContentStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryJournalStorage;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/** Tests of the {@link FeedStore} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private final ContentStorageDirect mContentStorage = new InMemoryContentStorage();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry mExtensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);

    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private Configuration mConfiguration;
    @Mock
    private StoreListener mListener;
    private FakeMainThreadRunner mMainThreadRunner;
    private FakeTaskQueue mTaskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        when(mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false))
                .thenReturn(false);
        mTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mTaskQueue.initialize(() -> {});
        mMainThreadRunner = FakeMainThreadRunner.runTasksImmediately();
        mFakeThreadUtils.enforceMainThread(false);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new FeedStore(mConfiguration, mTimingUtils, mExtensionRegistry, mContentStorage,
                new InMemoryJournalStorage(), mFakeThreadUtils, mTaskQueue, mFakeClock,
                mBasicLoggingApi, this.mMainThreadRunner);
    }

    @Test
    public void testSwitchToEphemeralMode() {
        FeedStore store = (FeedStore) getStore(mMainThreadRunner);
        assertThat(store.isEphemeralMode()).isFalse();
        store.switchToEphemeralMode();
        assertThat(store.isEphemeralMode()).isTrue();
        verify(mBasicLoggingApi).onInternalError(InternalFeedError.SWITCH_TO_EPHEMERAL);
    }

    @Test
    public void testSwitchToEphemeralMode_listeners() {
        FeedStore store = (FeedStore) getStore(mMainThreadRunner);
        assertThat(store.isEphemeralMode()).isFalse();

        store.registerObserver(mListener);

        store.switchToEphemeralMode();
        assertThat(store.isEphemeralMode()).isTrue();
        verify(mListener).onSwitchToEphemeralMode();
    }

    @Test
    public void testDumpEphemeralActions_notEphemeralMode() {
        JournalStorageDirect journalStorageSpy =
                spy(new DelegatingJournalStorage(new InMemoryJournalStorage()));
        ContentStorageDirect contentStorageSpy =
                spy(new DelegatingContentStorage(this.mContentStorage));
        FeedStore store = new FeedStore(mConfiguration, mTimingUtils, mExtensionRegistry,
                contentStorageSpy, journalStorageSpy, mFakeThreadUtils, mTaskQueue, mFakeClock,
                mBasicLoggingApi, mMainThreadRunner);
        store.onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
        verifyZeroInteractions(journalStorageSpy, contentStorageSpy);
    }

    @Test
    public void testDumpEphemeralActions_ephemeralMode() throws InvalidProtocolBufferException {
        JournalStorageDirect journalStorageSpy =
                spy(new DelegatingJournalStorage(new InMemoryJournalStorage()));
        ContentStorageDirect contentStorageSpy =
                spy(new DelegatingContentStorage(this.mContentStorage));
        FeedStore store = new FeedStore(mConfiguration, mTimingUtils, mExtensionRegistry,
                contentStorageSpy, journalStorageSpy, mFakeThreadUtils, mTaskQueue, mFakeClock,
                mBasicLoggingApi, mMainThreadRunner);
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
