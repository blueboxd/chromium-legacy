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

import com.google.search.now.feed.client.StreamDataProto.ClientBasicLoggingMetadata;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeatureProto.Feature.RenderableUnit;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;

/**
 * {@link DataOperationTransformer} for {@link Content} that adds {@link
 * ClientBasicLoggingMetadata}.
 */
public final class ContentDataOperationTransformer implements DataOperationTransformer {

  @Override
  public StreamDataOperation.Builder transform(
      DataOperation dataOperation,
      StreamDataOperation.Builder streamDataOperation,
      FeedResponseMetadata feedResponseMetadata) {
    if (dataOperation.getFeature().getRenderableUnit() != RenderableUnit.CONTENT) {
      return streamDataOperation;
    }
    Content content = dataOperation.getFeature().getExtension(Content.contentExtension);
    StreamFeature.Builder streamFeature =
        streamDataOperation.getStreamPayload().getStreamFeature().toBuilder().setContent(content);
    if (!feedResponseMetadata.hasResponseTimeMs()) {
      streamDataOperation.setStreamPayload(
          streamDataOperation.getStreamPayload().toBuilder().setStreamFeature(streamFeature));
      return streamDataOperation;
    }
    BasicLoggingMetadata.Builder basicLoggingData =
        content
            .getBasicLoggingMetadata()
            .toBuilder()
            .setExtension(
                ClientBasicLoggingMetadata.clientBasicLoggingMetadata,
                ClientBasicLoggingMetadata.newBuilder()
                    .setAvailabilityTimeSeconds(feedResponseMetadata.getResponseTimeMs())
                    .build());

    Content.Builder contentBuilder = content.toBuilder().setBasicLoggingMetadata(basicLoggingData);
    streamFeature = streamFeature.setContent(contentBuilder);
    streamDataOperation.setStreamPayload(
        streamDataOperation.getStreamPayload().toBuilder().setStreamFeature(streamFeature));

    return streamDataOperation;
  }
}
