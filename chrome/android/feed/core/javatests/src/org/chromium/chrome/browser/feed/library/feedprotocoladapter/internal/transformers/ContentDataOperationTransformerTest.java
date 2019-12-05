// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.ClientBasicLoggingMetadata;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content.Type;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature.RenderableUnit;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponseMetadata;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ContentDataOperationTransformer}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
        mContentDataOperationTransformer = new ContentDataOperationTransformer();
        mDataOperation = DataOperation.newBuilder();
        mStreamFeature = StreamFeature.newBuilder().setContentId(CONTENT_ID).build();
        mDataOperationBuilder = StreamDataOperation.newBuilder().setStreamPayload(
                StreamPayload.newBuilder().setStreamFeature(mStreamFeature));
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
        mDataOperation.setFeature(Feature.newBuilder()
                                          .setExtension(Content.contentExtension, content)
                                          .setRenderableUnit(RenderableUnit.CONTENT));

        StreamDataOperation.Builder operation = mContentDataOperationTransformer.transform(
                mDataOperation.build(), mDataOperationBuilder, METADATA);

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
        mDataOperation.setFeature(
                Feature.newBuilder()
                        .setExtension(Content.contentExtension, Content.getDefaultInstance())
                        .setRenderableUnit(RenderableUnit.CONTENT));
        StreamDataOperation.Builder operation =
                mContentDataOperationTransformer.transform(mDataOperation.build(),
                        mDataOperationBuilder, FeedResponseMetadata.getDefaultInstance());

        assertThat(operation).isSameInstanceAs(mDataOperationBuilder);
    }

    @Test
    public void transform_featureIsNotContent() {
        StreamDataOperation.Builder operation = mContentDataOperationTransformer.transform(
                mDataOperation.build(), mDataOperationBuilder, METADATA);

        assertThat(operation).isSameInstanceAs(mDataOperationBuilder);
    }
}
