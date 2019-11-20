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

package com.google.android.libraries.feed.feedprotocoladapter.internal.transformers;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Cluster;
import com.google.search.now.ui.stream.StreamStructureProto.Stream;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeatureProto.Feature;
import com.google.search.now.wire.feed.FeatureProto.Feature.RenderableUnit;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link FeatureDataOperationTransformer}. */
@RunWith(RobolectricTestRunner.class)
public class FeatureDataOperationTransformerTest {

  private static final String CONTENT_ID = "123";
  private DataOperation.Builder dataOperation;
  private StreamDataOperation.Builder dataOperationBuilder;
  private StreamFeature streamFeature;
  private FeatureDataOperationTransformer featureDataOperationTransformer;

  @Before
  public void setUp() {
    featureDataOperationTransformer = new FeatureDataOperationTransformer();
    dataOperation = DataOperation.newBuilder();
    streamFeature = StreamFeature.newBuilder().setContentId(CONTENT_ID).build();
    dataOperationBuilder =
        StreamDataOperation.newBuilder()
            .setStreamPayload(StreamPayload.newBuilder().setStreamFeature(streamFeature));
  }

  @Test
  public void testTransformSetStream() {
    Feature feature =
        Feature.newBuilder()
            .setRenderableUnit(RenderableUnit.STREAM)
            .setExtension(Stream.streamExtension, Stream.getDefaultInstance())
            .build();
    dataOperation.setFeature(feature);
    StreamFeature expectedStreamFeature =
        streamFeature.toBuilder().setStream(Stream.getDefaultInstance()).build();

    StreamDataOperation.Builder operation =
        featureDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

    assertThat(operation.getStreamPayload().getStreamFeature()).isEqualTo(expectedStreamFeature);
  }

  @Test
  public void testTransformSetCard() {
    Feature feature =
        Feature.newBuilder()
            .setRenderableUnit(RenderableUnit.CARD)
            .setExtension(Card.cardExtension, Card.getDefaultInstance())
            .build();
    dataOperation.setFeature(feature);
    StreamFeature expectedStreamFeature =
        streamFeature.toBuilder().setCard(Card.getDefaultInstance()).build();

    StreamDataOperation.Builder operation =
        featureDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

    assertThat(operation.getStreamPayload().getStreamFeature()).isEqualTo(expectedStreamFeature);
  }

  @Test
  public void testTransformSetCluster() {
    Feature feature =
        Feature.newBuilder()
            .setRenderableUnit(RenderableUnit.CLUSTER)
            .setExtension(Cluster.clusterExtension, Cluster.getDefaultInstance())
            .build();
    dataOperation.setFeature(feature);
    StreamFeature expectedStreamFeature =
        streamFeature.toBuilder().setCluster(Cluster.getDefaultInstance()).build();

    StreamDataOperation.Builder operation =
        featureDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

    assertThat(operation.getStreamPayload().getStreamFeature()).isEqualTo(expectedStreamFeature);
  }
}
