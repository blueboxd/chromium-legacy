// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    public StreamDataOperation.Builder transform(DataOperation dataOperation,
            StreamDataOperation.Builder streamDataOperation,
            FeedResponseMetadata feedResponseMetadata) {
        if (dataOperation.getFeature().getRenderableUnit() != RenderableUnit.CONTENT) {
            return streamDataOperation;
        }
        Content content = dataOperation.getFeature().getExtension(Content.contentExtension);
        StreamFeature.Builder streamFeature =
                streamDataOperation.getStreamPayload().getStreamFeature().toBuilder().setContent(
                        content);
        if (!feedResponseMetadata.hasResponseTimeMs()) {
            streamDataOperation.setStreamPayload(
                    streamDataOperation.getStreamPayload().toBuilder().setStreamFeature(
                            streamFeature));
            return streamDataOperation;
        }
        BasicLoggingMetadata.Builder basicLoggingData =
                content.getBasicLoggingMetadata().toBuilder().setExtension(
                        ClientBasicLoggingMetadata.clientBasicLoggingMetadata,
                        ClientBasicLoggingMetadata.newBuilder()
                                .setAvailabilityTimeSeconds(
                                        feedResponseMetadata.getResponseTimeMs())
                                .build());

        Content.Builder contentBuilder =
                content.toBuilder().setBasicLoggingMetadata(basicLoggingData);
        streamFeature = streamFeature.setContent(contentBuilder);
        streamDataOperation.setStreamPayload(
                streamDataOperation.getStreamPayload().toBuilder().setStreamFeature(streamFeature));

        return streamDataOperation;
    }
}
