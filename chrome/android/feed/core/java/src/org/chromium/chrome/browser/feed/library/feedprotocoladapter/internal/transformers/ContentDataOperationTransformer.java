// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.ClientBasicLoggingMetadata;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature.RenderableUnit;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponseMetadata;

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
