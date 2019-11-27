// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Cluster;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Stream;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponseMetadata;

/** {@link DataOperationTransformer} to populate {@link StreamFeature}. */
public final class FeatureDataOperationTransformer implements DataOperationTransformer {
    private static final String TAG = "FeatureDataOperationTra";

    @Override
    public StreamDataOperation.Builder transform(DataOperation dataOperation,
            StreamDataOperation.Builder dataOperationBuilder,
            FeedResponseMetadata feedResponseMetadata) {
        StreamFeature.Builder streamFeature =
                dataOperationBuilder.getStreamPayload().getStreamFeature().toBuilder();
        switch (dataOperation.getFeature().getRenderableUnit()) {
            case STREAM:
                streamFeature.setStream(
                        dataOperation.getFeature().getExtension(Stream.streamExtension));
                break;
            case CARD:
                streamFeature.setCard(dataOperation.getFeature().getExtension(Card.cardExtension));
                break;
            case CLUSTER:
                streamFeature.setCluster(
                        dataOperation.getFeature().getExtension(Cluster.clusterExtension));
                break;
            case CONTENT:
                // Content is handled in ContentDataOperationTransformer
                break;
            case TOKEN:
                // Tokens are handled in FeedProtocolAdapter
                break;
            default:
                Logger.e(TAG, "Unknown Feature payload %s, ignored",
                        dataOperation.getFeature().getRenderableUnit());
                return dataOperationBuilder;
        }
        dataOperationBuilder.setStreamPayload(
                dataOperationBuilder.getStreamPayload().toBuilder().setStreamFeature(
                        streamFeature));
        return dataOperationBuilder;
    }
}
