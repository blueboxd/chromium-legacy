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

import com.google.search.now.feed.client.StreamDataProto.ClientBasicLoggingMetadata;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.Content.Type;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeatureProto.Feature;
import com.google.search.now.wire.feed.FeatureProto.Feature.RenderableUnit;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ContentDataOperationTransformer}. */
@RunWith(RobolectricTestRunner.class)
public class ContentDataOperationTransformerTest {

  private static final String CONTENT_ID = "content-11";
  private static final long RESPONSE_TIME = 1000;
  private static final FeedResponseMetadata METADATA =
      FeedResponseMetadata.newBuilder().setResponseTimeMs(RESPONSE_TIME).build();
  private ContentDataOperationTransformer contentDataOperationTransformer;
  private DataOperation.Builder dataOperation;
  private StreamDataOperation.Builder dataOperationBuilder;
  private StreamFeature streamFeature;

  @Before
  public void setUp() {
    contentDataOperationTransformer = new ContentDataOperationTransformer();
    dataOperation = DataOperation.newBuilder();
    streamFeature = StreamFeature.newBuilder().setContentId(CONTENT_ID).build();
    dataOperationBuilder =
        StreamDataOperation.newBuilder()
            .setStreamPayload(StreamPayload.newBuilder().setStreamFeature(streamFeature));
  }

  @Test
  public void transform_setsContent() {
    BasicLoggingMetadata basicLoggingMetadata =
        BasicLoggingMetadata.newBuilder().setScore(.2f).build();
    ClientBasicLoggingMetadata clientBasicLoggingMetadata =
        ClientBasicLoggingMetadata.newBuilder().setAvailabilityTimeSeconds(RESPONSE_TIME).build();
    Content content =
        Content.newBuilder()
            .setType(Type.UNKNOWN_CONTENT)
            .setBasicLoggingMetadata(basicLoggingMetadata)
            .build();
    dataOperation.setFeature(
        Feature.newBuilder()
            .setExtension(Content.contentExtension, content)
            .setRenderableUnit(RenderableUnit.CONTENT));

    StreamDataOperation.Builder operation =
        contentDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, METADATA);

    assertThat(operation.getStreamPayload().getStreamFeature().getContent())
        .isEqualTo(
            content
                .toBuilder()
                .setBasicLoggingMetadata(
                    basicLoggingMetadata
                        .toBuilder()
                        .setExtension(
                            ClientBasicLoggingMetadata.clientBasicLoggingMetadata,
                            clientBasicLoggingMetadata)
                        .build())
                .build());
  }

  @Test
  public void transform_responseTimeNotSet() {
    dataOperation.setFeature(
        Feature.newBuilder()
            .setExtension(Content.contentExtension, Content.getDefaultInstance())
            .setRenderableUnit(RenderableUnit.CONTENT));
    StreamDataOperation.Builder operation =
        contentDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

    assertThat(operation).isSameInstanceAs(dataOperationBuilder);
  }

  @Test
  public void transform_featureIsNotContent() {
    StreamDataOperation.Builder operation =
        contentDataOperationTransformer.transform(
            dataOperation.build(), dataOperationBuilder, METADATA);

    assertThat(operation).isSameInstanceAs(dataOperationBuilder);
  }
}
