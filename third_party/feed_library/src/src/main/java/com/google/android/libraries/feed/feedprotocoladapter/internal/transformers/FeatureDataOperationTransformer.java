// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedprotocoladapter.internal.transformers;

import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Cluster;
import com.google.search.now.ui.stream.StreamStructureProto.Stream;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;

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
