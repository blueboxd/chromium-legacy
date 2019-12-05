// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Cluster;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Stream;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature.RenderableUnit;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponseMetadata;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link FeatureDataOperationTransformer}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeatureDataOperationTransformerTest {
    private static final String CONTENT_ID = "123";
    private DataOperation.Builder mDataOperation;
    private StreamDataOperation.Builder mDataOperationBuilder;
    private StreamFeature mStreamFeature;
    private FeatureDataOperationTransformer mFeatureDataOperationTransformer;

    @Before
    public void setUp() {
        mFeatureDataOperationTransformer = new FeatureDataOperationTransformer();
        mDataOperation = DataOperation.newBuilder();
        mStreamFeature = StreamFeature.newBuilder().setContentId(CONTENT_ID).build();
        mDataOperationBuilder = StreamDataOperation.newBuilder().setStreamPayload(
                StreamPayload.newBuilder().setStreamFeature(mStreamFeature));
    }

    @Test
    public void testTransformSetStream() {
        Feature feature = Feature.newBuilder()
                                  .setRenderableUnit(RenderableUnit.STREAM)
                                  .setExtension(Stream.streamExtension, Stream.getDefaultInstance())
                                  .build();
        mDataOperation.setFeature(feature);
        StreamFeature expectedStreamFeature =
                mStreamFeature.toBuilder().setStream(Stream.getDefaultInstance()).build();

        StreamDataOperation.Builder operation =
                mFeatureDataOperationTransformer.transform(mDataOperation.build(),
                        mDataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

        assertThat(operation.getStreamPayload().getStreamFeature())
                .isEqualTo(expectedStreamFeature);
    }

    @Test
    public void testTransformSetCard() {
        Feature feature = Feature.newBuilder()
                                  .setRenderableUnit(RenderableUnit.CARD)
                                  .setExtension(Card.cardExtension, Card.getDefaultInstance())
                                  .build();
        mDataOperation.setFeature(feature);
        StreamFeature expectedStreamFeature =
                mStreamFeature.toBuilder().setCard(Card.getDefaultInstance()).build();

        StreamDataOperation.Builder operation =
                mFeatureDataOperationTransformer.transform(mDataOperation.build(),
                        mDataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

        assertThat(operation.getStreamPayload().getStreamFeature())
                .isEqualTo(expectedStreamFeature);
    }

    @Test
    public void testTransformSetCluster() {
        Feature feature =
                Feature.newBuilder()
                        .setRenderableUnit(RenderableUnit.CLUSTER)
                        .setExtension(Cluster.clusterExtension, Cluster.getDefaultInstance())
                        .build();
        mDataOperation.setFeature(feature);
        StreamFeature expectedStreamFeature =
                mStreamFeature.toBuilder().setCluster(Cluster.getDefaultInstance()).build();

        StreamDataOperation.Builder operation =
                mFeatureDataOperationTransformer.transform(mDataOperation.build(),
                        mDataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

        assertThat(operation.getStreamPayload().getStreamFeature())
                .isEqualTo(expectedStreamFeature);
    }
}
