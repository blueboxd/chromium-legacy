// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.removetrackingfactory;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentRemoval;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.RemoveTracking;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests for {@link StreamRemoveTrackingFactory}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamRemoveTrackingFactoryTest {
    private static final String SESSION_ID = "sessionId";
    private static final String URL1 = "url1";
    private static final String URL2 = "url2";

    @Mock
    private FeedKnownContent mFeedKnownContent;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private KnownContent.Listener mKnownContentListener;

    private StreamRemoveTrackingFactory mStreamRemoveTrackingFactory;

    @Before
    public void setup() {
        initMocks(this);
        when(mModelProvider.getSessionId()).thenReturn(SESSION_ID);
        when(mFeedKnownContent.getKnownContentHostNotifier()).thenReturn(mKnownContentListener);
        mStreamRemoveTrackingFactory =
                new StreamRemoveTrackingFactory(mModelProvider, mFeedKnownContent);
    }

    @Test
    public void testCreate_nullRequestingSession() {
        assertThat(mStreamRemoveTrackingFactory.create(MutationContext.EMPTY_CONTEXT)).isNull();
    }

    @Test
    public void testCreate_otherSessionId() {
        assertThat(mStreamRemoveTrackingFactory.create(
                           new MutationContext.Builder().setRequestingSessionId("otherId").build()))
                .isNull();
    }

    @Test
    public void testRemoveTracking_userInitiated() {
        List<ContentRemoval> removedContents = triggerConsumerUpdatesFor(
                /* isUserInitiated= */ true, buildStreamFeatureForUrl(URL2),
                StreamFeature.getDefaultInstance(), buildStreamFeatureForUrl(URL1),
                StreamFeature.getDefaultInstance());

        assertThat(removedContents).hasSize(2);
        assertThat(removedContents.get(0).getUrl()).isEqualTo(URL2);
        assertThat(removedContents.get(1).getUrl()).isEqualTo(URL1);
        assertThat(removedContents.get(0).isRequestedByUser()).isTrue();
    }

    @Test
    public void testRemoveTracking_notUserInitiated() {
        List<ContentRemoval> removedContents = triggerConsumerUpdatesFor(
                /* isUserInitiated= */ false, buildStreamFeatureForUrl(URL1),
                buildStreamFeatureForUrl(URL2), StreamFeature.getDefaultInstance());

        assertThat(removedContents).hasSize(2);
        assertThat(removedContents.get(0).getUrl()).isEqualTo(URL1);
        assertThat(removedContents.get(1).getUrl()).isEqualTo(URL2);
        assertThat(removedContents.get(0).isRequestedByUser()).isFalse();
    }

    @SuppressWarnings("unchecked")
    private List<ContentRemoval> triggerConsumerUpdatesFor(
            boolean isUserInitiated, StreamFeature... streamFeatures) {
        RemoveTracking<ContentRemoval> contentRemovalRemoveTracking =
                mStreamRemoveTrackingFactory.create(createMutationContext(isUserInitiated));

        for (StreamFeature streamFeature : streamFeatures) {
            contentRemovalRemoveTracking.filterStreamFeature(streamFeature);
        }

        contentRemovalRemoveTracking.triggerConsumerUpdate();

        ArgumentCaptor<List<ContentRemoval>> removedContentCaptor =
                (ArgumentCaptor<List<ContentRemoval>>) (Object) ArgumentCaptor.forClass(List.class);

        verify(mKnownContentListener).onContentRemoved(removedContentCaptor.capture());
        return removedContentCaptor.getValue();
    }

    private StreamFeature buildStreamFeatureForUrl(String url) {
        return StreamFeature.newBuilder()
                .setContent(Content.newBuilder().setRepresentationData(
                        RepresentationData.newBuilder().setUri(url)))
                .build();
    }

    private MutationContext createMutationContext(boolean isUserInitiated) {
        return new MutationContext.Builder()
                .setRequestingSessionId(SESSION_ID)
                .setUserInitiated(isUserInitiated)
                .build();
    }
}
