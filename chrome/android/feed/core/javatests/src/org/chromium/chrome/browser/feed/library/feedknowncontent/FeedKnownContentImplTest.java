// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedknowncontent;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentRemoval;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.OfflineMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests for {@link FeedKnownContentImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedKnownContentImplTest {
    private static final long CONTENT_CREATION_DATE_TIME_MS = 123L;
    private static final List<ContentRemoval> CONTENT_REMOVED =
            Collections.singletonList(new ContentRemoval("url", /* requestedByUser= */ false));
    private static final String URL = "url";
    private static final String TITLE = "title";

    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private KnownContent.Listener mListener1;
    @Mock
    private KnownContent.Listener mListener2;
    @Mock
    private Consumer<List<ContentMetadata>> mKnownContentConsumer;
    @Mock
    private ThreadUtils mThreadUtils;

    private final FakeMainThreadRunner mMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    @Captor
    private ArgumentCaptor<Function<StreamPayload, ContentMetadata>> mKnownContentFunctionCaptor;

    @Captor
    private ArgumentCaptor<Consumer<Result<List<ContentMetadata>>>> mContentMetadataResultCaptor;

    private FeedKnownContentImpl mKnownContentApi;

    @Before
    public void setUp() {
        initMocks(this);
        when(mThreadUtils.isMainThread()).thenReturn(true);
        mKnownContentApi =
                new FeedKnownContentImpl(mFeedSessionManager, mMainThreadRunner, mThreadUtils);
    }

    @Test
    public void testSetsListenerOnSessionManager() {
        verify(mFeedSessionManager)
                .setKnownContentListener(mKnownContentApi.getKnownContentHostNotifier());
    }

    @Test
    public void testNotifyListeners_contentReceived() {
        mKnownContentApi.addListener(mListener1);
        mKnownContentApi.addListener(mListener2);

        mKnownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);

        verify(mListener1)
                .onNewContentReceived(
                        /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
        verify(mListener2)
                .onNewContentReceived(
                        /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
    }

    @Test
    public void testNotifyListeners_contentRemoved() {
        mKnownContentApi.addListener(mListener1);
        mKnownContentApi.addListener(mListener2);

        mKnownContentApi.getKnownContentHostNotifier().onContentRemoved(CONTENT_REMOVED);

        verify(mListener1).onContentRemoved(CONTENT_REMOVED);
        verify(mListener2).onContentRemoved(CONTENT_REMOVED);
    }

    @Test
    public void testRemoveListener() {
        mKnownContentApi.addListener(mListener1);
        mKnownContentApi.removeListener(mListener1);

        mKnownContentApi.getKnownContentHostNotifier().onContentRemoved(CONTENT_REMOVED);
        mKnownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ true, CONTENT_CREATION_DATE_TIME_MS);

        verifyNoMoreInteractions(mListener1);
    }

    @Test
    public void testGetKnownContent_returnsNullForNonContent() {
        mKnownContentApi.getKnownContent(mKnownContentConsumer);

        verify(mFeedSessionManager)
                .getStreamFeaturesFromHead(mKnownContentFunctionCaptor.capture(),
                        mContentMetadataResultCaptor.capture());

        assertThat(mKnownContentFunctionCaptor.getValue().apply(StreamPayload.getDefaultInstance()))
                .isNull();
    }

    @Test
    public void testGetKnownContent_returnsContentMetadataFromContent() {
        mKnownContentApi.getKnownContent(mKnownContentConsumer);

        verify(mFeedSessionManager)
                .getStreamFeaturesFromHead(mKnownContentFunctionCaptor.capture(),
                        mContentMetadataResultCaptor.capture());

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
                mKnownContentFunctionCaptor.getValue().apply(streamPayload);

        assertThat(contentMetadata.getUrl()).isEqualTo(URL);
        assertThat(contentMetadata.getTitle()).isEqualTo(TITLE);
    }

    @Test
    public void testGetKnownContent_failure() {
        mKnownContentApi.getKnownContent(mKnownContentConsumer);

        verify(mFeedSessionManager)
                .getStreamFeaturesFromHead(mKnownContentFunctionCaptor.capture(),
                        mContentMetadataResultCaptor.capture());

        mContentMetadataResultCaptor.getValue().accept(Result.failure());

        verify(mKnownContentConsumer, never())
                .accept(ArgumentMatchers.<List<ContentMetadata>>any());
    }

    @Test
    public void testGetKnownContent_offMainThread() {
        FakeMainThreadRunner fakeMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
        when(mThreadUtils.isMainThread()).thenReturn(false);

        mKnownContentApi =
                new FeedKnownContentImpl(mFeedSessionManager, fakeMainThreadRunner, mThreadUtils);

        mKnownContentApi.addListener(mListener1);

        mKnownContentApi.getKnownContentHostNotifier().onNewContentReceived(
                /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);

        assertThat(fakeMainThreadRunner.hasTasks()).isTrue();

        verifyZeroInteractions(mListener1);

        fakeMainThreadRunner.runAllTasks();

        verify(mListener1)
                .onNewContentReceived(
                        /* isNewRefresh= */ false, CONTENT_CREATION_DATE_TIME_MS);
    }
}
