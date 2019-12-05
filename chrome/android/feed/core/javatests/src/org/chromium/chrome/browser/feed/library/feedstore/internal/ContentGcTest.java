// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.UPLOADABLE_ACTION_PREFIX;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeDirectExecutor;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link ContentGc} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentGcTest {
    @Captor
    private ArgumentCaptor<ContentMutation> mContentMutationCaptor;
    @Mock
    private ContentStorageDirect mContentStorage;

    private static final String CONTENT_ID_1 = "contentId1";
    private static final String CONTENT_ID_2 = "contentId2";
    private static final String SEMANTIC_PROPERTIES_1 = SEMANTIC_PROPERTIES_PREFIX + CONTENT_ID_1;
    private static final String SEMANTIC_PROPERTIES_2 = SEMANTIC_PROPERTIES_PREFIX + CONTENT_ID_2;
    private static final String ACTION_1 = UPLOADABLE_ACTION_PREFIX + CONTENT_ID_1;
    private static final long MAXIMUM_GC_ATTEMPTS = 3L;

    private final Configuration mConfiguration =
            new Configuration.Builder()
                    .put(ConfigKey.MAXIMUM_GC_ATTEMPTS, MAXIMUM_GC_ATTEMPTS)
                    .build();
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FakeDirectExecutor mFakeDirectExecutor =
            FakeDirectExecutor.queueAllTasks(mFakeThreadUtils);
    private final TimingUtils mTimingUtils = new TimingUtils();

    private FakeTaskQueue mFakeTaskQueue;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeDirectExecutor);
        mFakeTaskQueue.initialize(() -> {});
    }

    @Test
    public void gc() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations = new ContentMutation.Builder()
                                                            .delete(CONTENT_ID_1)
                                                            .delete(CONTENT_ID_2)
                                                            .build()
                                                            .getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_accessible() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_reserved() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc =
                new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(CONTENT_ID_1),
                        ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                        /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_actionUploads_validAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(ACTION_1);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_noAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations = new ContentMutation.Builder()
                                                            .delete(SEMANTIC_PROPERTIES_1)
                                                            .delete(SEMANTIC_PROPERTIES_2)
                                                            .build()
                                                            .getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_validAction() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ()
                        -> ImmutableSet.of(StreamLocalAction.newBuilder()
                                                   .setAction(ActionType.DISMISS)
                                                   .setFeatureContentId(CONTENT_ID_1)
                                                   .build()),
                mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(SEMANTIC_PROPERTIES_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_semanticProperties_accessibleContent() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(CONTENT_ID_1);
        contentKeys.add(SEMANTIC_PROPERTIES_1);
        contentKeys.add(SEMANTIC_PROPERTIES_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(SEMANTIC_PROPERTIES_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_prefixed_sharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ true);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_deleteSharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder()
                        .delete(SHARED_STATE_PREFIX + CONTENT_ID_1)
                        .delete(CONTENT_ID_2)
                        .build()
                        .getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_keepAccessibleSharedState() {
        List<String> contentKeys = new ArrayList<>();
        contentKeys.add(SHARED_STATE_PREFIX + CONTENT_ID_1);
        contentKeys.add(CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration,
                ()
                        -> ImmutableSet.of(CONTENT_ID_1),
                ImmutableSet.of(), ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
        List<ContentOperation> expectedOperations =
                new ContentMutation.Builder().delete(CONTENT_ID_2).build().getOperations();
        List<ContentOperation> resultOperations = mContentMutationCaptor.getValue().getOperations();
        assertListsContainSameElements(expectedOperations, resultOperations);
    }

    @Test
    public void gc_delayWhileTaskEnqueued() {
        mFakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();

        assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(2);
        verify(mContentStorage, never()).commit(mContentMutationCaptor.capture());
    }

    @Test
    public void gc_runWhenMaximumAttemptsReached() {
        mFakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(mConfiguration, ImmutableSet::of, ImmutableSet.of(),
                ImmutableSet::of, mContentStorage, mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ false);
        for (int i = 0; i < MAXIMUM_GC_ATTEMPTS; i++) {
            contentGc.gc();
            assertThat(mFakeTaskQueue.getBackgroundTaskCount()).isEqualTo(2 + i);
            verify(mContentStorage, never()).commit(mContentMutationCaptor.capture());
        }

        contentGc.gc();
        verify(mContentStorage).commit(mContentMutationCaptor.capture());
    }

    @Test
    public void gc_runWhenMaximumIsZero() {
        mFakeTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, () -> {});
        List<String> contentKeys = ImmutableList.of(CONTENT_ID_1, CONTENT_ID_2);
        mockContentStorageWithContents(contentKeys);
        ContentGc contentGc = new ContentGc(
                new Configuration.Builder().put(ConfigKey.MAXIMUM_GC_ATTEMPTS, 0L).build(),
                ImmutableSet::of, ImmutableSet.of(), ImmutableSet::of, mContentStorage,
                mFakeTaskQueue, mTimingUtils,
                /* keepSharedStates= */ false);
        contentGc.gc();

        verify(mContentStorage).commit(mContentMutationCaptor.capture());
    }

    private void mockContentStorageWithContents(List<String> contentKeys) {
        when(mContentStorage.getAllKeys()).thenReturn(Result.success(contentKeys));
        when(mContentStorage.commit(any(ContentMutation.class))).thenReturn(CommitResult.SUCCESS);
    }

    private void assertListsContainSameElements(
            List<ContentOperation> expectedOperations, List<ContentOperation> resultOperations) {
        assertThat(resultOperations.size()).isEqualTo(expectedOperations.size());
        assertThat(resultOperations.containsAll(expectedOperations)).isTrue();
    }
}
