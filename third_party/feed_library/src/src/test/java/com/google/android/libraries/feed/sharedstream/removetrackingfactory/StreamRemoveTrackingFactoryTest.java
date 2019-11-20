// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.sharedstream.removetrackingfactory;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.RemoveTracking;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link StreamRemoveTrackingFactory}. */
@RunWith(RobolectricTestRunner.class)
public class StreamRemoveTrackingFactoryTest {

  private static final String SESSION_ID = "sessionId";
  private static final String URL1 = "url1";
  private static final String URL2 = "url2";

  @Mock private FeedKnownContent feedKnownContent;
  @Mock private ModelProvider modelProvider;
  @Mock private KnownContent.Listener knownContentListener;

  private StreamRemoveTrackingFactory streamRemoveTrackingFactory;

  @Before
  public void setup() {
    initMocks(this);
    when(modelProvider.getSessionId()).thenReturn(SESSION_ID);
    when(feedKnownContent.getKnownContentHostNotifier()).thenReturn(knownContentListener);
    streamRemoveTrackingFactory = new StreamRemoveTrackingFactory(modelProvider, feedKnownContent);
  }

  @Test
  public void testCreate_nullRequestingSession() {
    assertThat(streamRemoveTrackingFactory.create(MutationContext.EMPTY_CONTEXT)).isNull();
  }

  @Test
  public void testCreate_otherSessionId() {
    assertThat(
            streamRemoveTrackingFactory.create(
                new MutationContext.Builder().setRequestingSessionId("otherId").build()))
        .isNull();
  }

  @Test
  public void testRemoveTracking_userInitiated() {
    List<ContentRemoval> removedContents =
        triggerConsumerUpdatesFor(
            /* isUserInitiated= */ true,
            buildStreamFeatureForUrl(URL2),
            StreamFeature.getDefaultInstance(),
            buildStreamFeatureForUrl(URL1),
            StreamFeature.getDefaultInstance());

    assertThat(removedContents).hasSize(2);
    assertThat(removedContents.get(0).getUrl()).isEqualTo(URL2);
    assertThat(removedContents.get(1).getUrl()).isEqualTo(URL1);
    assertThat(removedContents.get(0).isRequestedByUser()).isTrue();
  }

  @Test
  public void testRemoveTracking_notUserInitiated() {
    List<ContentRemoval> removedContents =
        triggerConsumerUpdatesFor(
            /* isUserInitiated= */ false,
            buildStreamFeatureForUrl(URL1),
            buildStreamFeatureForUrl(URL2),
            StreamFeature.getDefaultInstance());

    assertThat(removedContents).hasSize(2);
    assertThat(removedContents.get(0).getUrl()).isEqualTo(URL1);
    assertThat(removedContents.get(1).getUrl()).isEqualTo(URL2);
    assertThat(removedContents.get(0).isRequestedByUser()).isFalse();
  }

  @SuppressWarnings("unchecked")
  private List<ContentRemoval> triggerConsumerUpdatesFor(
      boolean isUserInitiated, StreamFeature... streamFeatures) {
    RemoveTracking<ContentRemoval> contentRemovalRemoveTracking =
        streamRemoveTrackingFactory.create(createMutationContext(isUserInitiated));

    for (StreamFeature streamFeature : streamFeatures) {
      contentRemovalRemoveTracking.filterStreamFeature(streamFeature);
    }

    contentRemovalRemoveTracking.triggerConsumerUpdate();

    ArgumentCaptor<List<ContentRemoval>> removedContentCaptor =
        (ArgumentCaptor<List<ContentRemoval>>) (Object) ArgumentCaptor.forClass(List.class);

    verify(knownContentListener).onContentRemoved(removedContentCaptor.capture());
    return removedContentCaptor.getValue();
  }

  private StreamFeature buildStreamFeatureForUrl(String url) {
    return StreamFeature.newBuilder()
        .setContent(
            Content.newBuilder().setRepresentationData(RepresentationData.newBuilder().setUri(url)))
        .build();
  }

  private MutationContext createMutationContext(boolean isUserInitiated) {
    return new MutationContext.Builder()
        .setRequestingSessionId(SESSION_ID)
        .setUserInitiated(isUserInitiated)
        .build();
  }
}
