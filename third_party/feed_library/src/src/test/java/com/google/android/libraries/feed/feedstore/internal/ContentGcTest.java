// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.feedstore.internal;

import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;
import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;
import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.UPLOADABLE_ACTION_PREFIX;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentMutation.Builder;
import com.google.android.libraries.feed.api.host.storage.ContentOperation;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.concurrent.testing.FakeDirectExecutor;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link ContentGc} class. */
@RunWith(RobolectricTestRunner.class)
public class ContentGcTest {
    @Captor
    private ArgumentCaptor<ContentMutation> contentMutationCaptor;
    @Mock
    private ContentStorageDirect contentStorage;

    private static final String CONTENT_ID_1 = "contentId1";
    private static final String CONTENT_ID_2 = "contentId2";
    private static final String SEMANTIC_PROPERTIES_1 = SEMANTIC_PROPERTIES_PREFIX + CONTENT_ID_1;
    private static final String SEMANTIC_PROPERTIES_2 = SEMANTIC_PROPERTIES_PREFIX + CONTENT_ID_2;
    private static final String ACTION_1 = UPLOADABLE_ACTION_PREFIX + CONTENT_ID_1;
    private static final long MAXIMUM_GC_ATTEMPTS = 3L;

    private final Configuration configuration =
            new Configuration.Builder()
                    .put(ConfigKey.MAXIMUM_GC_ATTEMPTS, MAXIMUM_GC_ATTEMPTS)
                    .build();
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FakeDirectExecutor fakeDirectExecutor =
            FakeDirectExecutor.queueAllTasks(fakeThreadUtils);
    private final TimingUtils timingUtils = new TimingUtils();

    private FakeTaskQueue fakeTaskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeDirectExecutor);
        fakeTaskQueue.initialize(() -> {});
    }

    @Test
    public void gc() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(CONTENT_ID_1).delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_accessible() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_reserved() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc =
                new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(CONTENT_ID_1),
                        ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                        /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_actionUploads_validAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(ACTION_1);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations = new Builder().build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_noAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations = new Builder()
                                                            .delete(SEMANTIC_PROPERTIES_1)
                                                            .delete(SEMANTIC_PROPERTIES_2)
                                                            .build()
                                                            .getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_validAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ()
                        -> ImmutableSet.of(StreamLocalAction.newBuilder()
                                                   .setAction(ActionType.DISMISS)
                                                   .setFeatureContentId(CONTENT_ID_1)
                                                   .build()),
                contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(SEMANTIC_PROPERTIES_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_accessibleContent() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(SEMANTIC_PROPERTIES_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_prefixed_sharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_deleteSharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder()
                        .delete(SHARED_STATE_PREFIX + CONTENT_ID_1)
                        .delete(CONTENT_ID_2)
                        .build()
                        .getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_keepAccessibleSharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = contentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_delayWhileTaskEnqueued() {
        fakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();

        assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(2);
        verify(contentStorage, never()).commit(contentMutationCaptor.capture());
    }

    @Test
    public void gc_runWhenMaximumAttemptsReached() {
        fakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(configuration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, contentStorage, fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ false);
        for (int i = 0; i < MAXIMUM_GC_ATTEMPTS; i++) {
            contentGc.gc();
            assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(2 + i);
            verify(contentStorage, never()).commit(contentMutationCaptor.capture());
        }

        contentGc.gc();
        verify(contentStorage).commit(contentMutationCaptor.capture());
    }

    @Test
    public void gc_runWhenMaximumIsZero() {
        fakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(
                new Configuration.Builder().put(ConfigKey.MAXIMUM_GC_ATTEMPTS, 0L).build(),
                ImmutableSet::of, ImmutableSet.of(), ImmutableSet::of, contentStorage,
                fakeTaskQueue, timingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();

        verify(contentStorage).commit(contentMutationCaptor.capture());
    }

    private void mockContentStorageWithContents(List<String> contentKeys) {
        when(contentStorage.getAllKeys()).thenReturn(Result.success(contentKeys));
        when(contentStorage.commit(any(ContentMutation.class))).thenReturn(CommitResult.SUCCESS);
    }

    private void assertListsContainSameElements(
            List<ContentOperation> expectedOperations, List<ContentOperation> resultOperations) {
        assertThat(resultOperations.size()).isEqualTo(expectedOperations.size());
        assertThat(resultOperations.containsAll(expectedOperations)).isTrue();
    }
}
