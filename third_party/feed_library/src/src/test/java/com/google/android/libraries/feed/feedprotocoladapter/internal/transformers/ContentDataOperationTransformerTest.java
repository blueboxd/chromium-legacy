// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    private ContentDataOperationTransformer mContentDataOperationTransformer;
    private DataOperation.Builder mDataOperation;
    private StreamDataOperation.Builder mDataOperationBuilder;
    private StreamFeature mStreamFeature;

    @Before
    public void setUp() {
        contentDataOperationTransformer = new ContentDataOperationTransformer();
        dataOperation = DataOperation.newBuilder();
        streamFeature = StreamFeature.newBuilder().setContentId(CONTENT_ID).build();
        dataOperationBuilder = StreamDataOperation.newBuilder().setStreamPayload(
                StreamPayload.newBuilder().setStreamFeature(streamFeature));
    }

    @Test
    public void transform_setsContent() {
        BasicLoggingMetadata basicLoggingMetadata =
                BasicLoggingMetadata.newBuilder().setScore(.2f).build();
        ClientBasicLoggingMetadata clientBasicLoggingMetadata =
                ClientBasicLoggingMetadata.newBuilder()
                        .setAvailabilityTimeSeconds(RESPONSE_TIME)
                        .build();
        Content content = Content.newBuilder()
                                  .setType(Type.UNKNOWN_CONTENT)
                                  .setBasicLoggingMetadata(basicLoggingMetadata)
                                  .build();
        dataOperation.setFeature(Feature.newBuilder()
                                         .setExtension(Content.contentExtension, content)
                                         .setRenderableUnit(RenderableUnit.CONTENT));

        StreamDataOperation.Builder operation = contentDataOperationTransformer.transform(
                dataOperation.build(), dataOperationBuilder, METADATA);

        assertThat(operation.getStreamPayload().getStreamFeature().getContent())
                .isEqualTo(
                        content.toBuilder()
                                .setBasicLoggingMetadata(
                                        basicLoggingMetadata.toBuilder()
                                                .setExtension(ClientBasicLoggingMetadata
                                                                      .clientBasicLoggingMetadata,
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
                contentDataOperationTransformer.transform(dataOperation.build(),
                        dataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

        assertThat(operation).isSameInstanceAs(dataOperationBuilder);
    }

    @Test
    public void transform_featureIsNotContent() {
        StreamDataOperation.Builder operation = contentDataOperationTransformer.transform(
                dataOperation.build(), dataOperationBuilder, METADATA);

        assertThat(operation).isSameInstanceAs(dataOperationBuilder);
    }
}
