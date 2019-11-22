// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedknowncontent;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Function;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.OfflineMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests for {@link FeedKnownContentImpl}. */
@RunWith(RobolectricTestRunner.class)
public class FeedKnownContentImplTest {
    private static final long CONTENT_CREATION_DATE_TIME_MS = 123L;
    private static final List<ContentRemoval> CONTENT_REMOVED =
            Collections.singletonList(new ContentRemoval("url", /* requestedByUser= */ false));
    private static final String URL = "url";
    private static final String TITLE = "title";

    @Mock
    private FeedSessionManager feedSessionManager;
    @Mock
    private KnownContent.Listener listener1;
    @Mock
    private KnownContent.Listener listener2;
    @Mock
    private Consumer<List<ContentMetadata>> knownContentConsumer;
    @Mock
    private ThreadUtils threadUtils;

    private final FakeMainThreadRunner mainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    @Captor
    private ArgumentCaptor<Function<StreamPayload, ContentMetadata>> knownContentFunctionCaptor;

    @Captor
    private ArgumentCaptor<Consumer<Result<List<ContentMetadata>>>> contentMetadataResultCaptor;

    private FeedKnownContentImpl knownContentApi;

    @Before
    public void setUp() {
        initMocks(this);
        when(threadUtils.isMainThread()).thenReturn(true);
        knownContentApi =
                new FeedKnownContentImpl(feedSessionManager, mainThreadRunner, threadUtils);
    }

    @Test
    public void testSetsListenerOnSessionManager() {
        verify(feedSessionManager)
                .setKnownContentListener(knownContentApi.getKnownContentHostNotifier());
    }

    @Test
    public void testNotifyListeners_contentReceived() {
        knownContentApi.addListener(listener1);
        knownContentApi.addListener(listener2);

        knownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);

        verify(listener1).onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
        verify(listener2).onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
    }

    @Test
    public void testNotifyListeners_contentRemoved() {
        knownContentApi.addListener(listener1);
        knownContentApi.addListener(listener2);

        knownContentApi.getKnownContentHostNotifier().onContentRemoved(CONTENT_REMOVED);

        verify(listener1).onContentRemoved(CONTENT_REMOVED);
        verify(listener2).onContentRemoved(CONTENT_REMOVED);
    }

    @Test
    public void testRemoveListener() {
        knownContentApi.addListener(listener1);
        knownContentApi.removeListener(listener1);

        knownContentApi.getKnownContentHostNotifier().onContentRemoved(CONTENT_REMOVED);
        knownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ true, CONTENT_CREATION_DATE_TIME_MS);

        verifyNoMoreInteractions(listener1);
    }

    @Test
    public void testGetKnownContent_returnsNullForNonContent() {
        knownContentApi.getKnownContent(knownContentConsumer);

        verify(feedSessionManager)
                .getStreamFeaturesFromHead(knownContentFunctionCaptor.capture(),
                        contentMetadataResultCaptor.capture());

        assertThat(knownContentFunctionCaptor.getValue().apply(StreamPayload.getDefaultInstance()))
                .isNull();
    }

    @Test
    public void testGetKnownContent_returnsContentMetadataFromContent() {
        knownContentApi.getKnownContent(knownContentConsumer);

        verify(feedSessionManager)
                .getStreamFeaturesFromHead(knownContentFunctionCaptor.capture(),
                        contentMetadataResultCaptor.capture());

        StreamPayload streamPayload =
                StreamPayload.newBuilder()
                        .setStreamFeature(StreamFeature.newBuilder().setContent(
                                Content.newBuilder()
                                        .setOfflineMetadata(
                                                OfflineMetadata.newBuilder().setTitle(TITLE))
                                        .setRepresentationData(
                                                RepresentationData.newBuilder().setUri(URL))))
                        .build();

        ContentMetadata contentMetadata =
                knownContentFunctionCaptor.getValue().apply(streamPayload);

        assertThat(contentMetadata.getUrl()).isEqualTo(URL);
        assertThat(contentMetadata.getTitle()).isEqualTo(TITLE);
    }

    @Test
    public void testGetKnownContent_failure() {
        knownContentApi.getKnownContent(knownContentConsumer);

        verify(feedSessionManager)
                .getStreamFeaturesFromHead(knownContentFunctionCaptor.capture(),
                        contentMetadataResultCaptor.capture());

        contentMetadataResultCaptor.getValue().accept(Result.failure());

        verify(knownContentConsumer, never()).accept(ArgumentMatchers.<List<ContentMetadata>>any());
    }

    @Test
    public void testGetKnownContent_offMainThread() {
        FakeMainThreadRunner fakeMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
        when(threadUtils.isMainThread()).thenReturn(false);

        knownContentApi =
                new FeedKnownContentImpl(feedSessionManager, fakeMainThreadRunner, threadUtils);

        knownContentApi.addListener(listener1);

        knownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);

        assertThat(fakeMainThreadRunner.hasTasks()).isTrue();

        verifyZeroInteractions(listener1);

        fakeMainThreadRunner.runAllTasks();

        verify(listener1).onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
    }
}
