// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.RequiredContentAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.nio.charset.Charset;
import java.util.List;

/** Tests of the {@link FeedProtocolAdapter} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedProtocolAdapterTest {
    private final TimingUtils mTimingUtils = new TimingUtils();
    private final FeedProtocolAdapter mProtocolAdapter =
            new FeedProtocolAdapter(ImmutableList.of(), mTimingUtils);

    @Mock
    private RequiredContentAdapter mAdapter;
    private ResponseBuilder mResponseBuilder;

    @Before
    public void init() {
        initMocks(this);
        mResponseBuilder = new ResponseBuilder();
    }

    @Test
    public void testConvertContentId() {
        ContentId contentId = ResponseBuilder.createFeatureContentId(13);
        String streamContentId = mProtocolAdapter.getStreamContentId(contentId);
        assertThat(streamContentId).isNotNull();
        assertThat(streamContentId).contains(contentId.getContentDomain());
        assertThat(streamContentId).contains(Long.toString(contentId.getId()));
        assertThat(streamContentId).contains(contentId.getTable());
    }

    @Test
    public void testConvertContentId_malformed_notThreeParts() {
        String streamContentId = "test" + FeedProtocolAdapter.CONTENT_ID_DELIMITER + "break";
        Result<ContentId> contentIdResult = mProtocolAdapter.getWireContentId(streamContentId);
        assertThat(contentIdResult.isSuccessful()).isFalse();
    }

    @Test
    public void testConvertContentId_malformed_nonNumericId() {
        String streamContentId = "test" + FeedProtocolAdapter.CONTENT_ID_DELIMITER + "break"
                + FeedProtocolAdapter.CONTENT_ID_DELIMITER + "test";
        Result<ContentId> contentIdResult = mProtocolAdapter.getWireContentId(streamContentId);
        assertThat(contentIdResult.isSuccessful()).isFalse();
    }

    @Test
    public void testConvertContentId_roundTrip() {
        ContentId contentId = ResponseBuilder.createFeatureContentId(13);
        String streamContentId = mProtocolAdapter.getStreamContentId(contentId);
        Result<ContentId> contentIdResult = mProtocolAdapter.getWireContentId(streamContentId);
        assertThat(contentIdResult.isSuccessful()).isTrue();
        assertThat(contentIdResult.getValue()).isNotNull();
        assertThat(contentIdResult.getValue()).isEqualTo(contentId);
    }

    @Test
    public void testConvertContentId_roundTrip_partialContentId() {
        ContentId contentId = ContentId.newBuilder().setId(13).build();
        String streamContentId = mProtocolAdapter.getStreamContentId(contentId);
        Result<ContentId> contentIdResult = mProtocolAdapter.getWireContentId(streamContentId);
        assertThat(contentIdResult.isSuccessful()).isTrue();
        assertThat(contentIdResult.getValue()).isNotNull();
        assertThat(contentIdResult.getValue()).isEqualTo(contentId);
    }

    @Test
    public void testSimpleResponse_clear() {
        Response response = mResponseBuilder.addClearOperation().build();
        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue().streamDataOperations).hasSize(1);
    }

    @Test
    public void testSimpleResponse_feature() {
        Response response = mResponseBuilder.addRootFeature().build();

        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue().streamDataOperations).hasSize(1);

        StreamDataOperation sdo = results.getValue().streamDataOperations.get(0);
        assertThat(sdo.hasStreamPayload()).isTrue();
        assertThat(sdo.getStreamPayload().hasStreamFeature()).isTrue();
        assertThat(sdo.hasStreamStructure()).isTrue();
        assertThat(sdo.getStreamStructure().hasContentId()).isTrue();
        // Added the root
        assertThat(sdo.getStreamStructure().hasParentContentId()).isFalse();
    }

    @Test
    public void testSimpleResponse_feature_semanticProperties() {
        ContentId contentId = ResponseBuilder.createFeatureContentId(13);
        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");
        Response response =
                new ResponseBuilder().addCardWithSemanticData(contentId, semanticData).build();

        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        // Note that 2 operations are created (the card and the semantic data). We want the latter.
        assertThat(results.getValue().streamDataOperations).hasSize(2);
        StreamDataOperation sdo = results.getValue().streamDataOperations.get(1);
        assertThat(sdo.getStreamPayload().hasSemanticData()).isTrue();
        assertThat(sdo.getStreamPayload().getSemanticData()).isEqualTo(semanticData);
    }

    @Test
    public void testResponse_rootClusterCardContent() {
        ContentId rootId = ContentId.newBuilder().setId(1).build();
        ContentId clusterId = ContentId.newBuilder().setId(2).build();
        ContentId cardId = ContentId.newBuilder().setId(3).build();
        Response response = mResponseBuilder.addRootFeature(rootId)
                                    .addClusterFeature(clusterId, rootId)
                                    .addCard(cardId, clusterId)
                                    .build();

        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        List<StreamDataOperation> operations = results.getValue().streamDataOperations;

        assertThat(operations).hasSize(4);
        assertThat(operations.get(0).getStreamPayload().getStreamFeature().hasStream()).isTrue();
        assertThat(operations.get(1).getStreamPayload().getStreamFeature().hasCluster()).isTrue();
        assertThat(operations.get(2).getStreamPayload().getStreamFeature().hasCard()).isTrue();
        assertThat(operations.get(3).getStreamPayload().getStreamFeature().hasContent()).isTrue();
    }

    @Test
    public void testResponse_remove() {
        Response response = mResponseBuilder
                                    .removeFeature(ContentId.getDefaultInstance(),
                                            ContentId.getDefaultInstance())
                                    .build();
        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue().streamDataOperations).hasSize(1);
    }

    @Test
    public void testPietSharedState() {
        Response response = mResponseBuilder.addPietSharedState().build();
        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue().streamDataOperations).hasSize(1);
        StreamDataOperation sdo = results.getValue().streamDataOperations.get(0);
        assertThat(sdo.hasStreamPayload()).isTrue();
        assertThat(sdo.getStreamPayload().hasStreamSharedState()).isTrue();
        assertThat(sdo.hasStreamStructure()).isTrue();
        assertThat(sdo.getStreamStructure().hasContentId()).isTrue();
    }

    @Test
    public void testContinuationToken_nextPageToken() {
        ByteString tokenForMutation = ByteString.copyFrom("token", Charset.defaultCharset());
        Response response = mResponseBuilder.addStreamToken(1, tokenForMutation).build();

        Result<Model> results = mProtocolAdapter.createModel(response);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue().streamDataOperations).hasSize(1);
        StreamDataOperation sdo = results.getValue().streamDataOperations.get(0);
        assertThat(sdo.hasStreamPayload()).isTrue();
        assertThat(sdo.getStreamPayload().hasStreamToken()).isTrue();
    }

    @Test
    public void testRequiredContentAdapter() {
        when(mAdapter.determineRequiredContentIds(any(DataOperation.class)))
                .thenReturn(ImmutableList.of(ContentId.newBuilder().setId(1).build()))
                .thenReturn(ImmutableList.of(ContentId.newBuilder().setId(2).build()))
                .thenReturn(ImmutableList.of(ContentId.newBuilder().setId(1).build(),
                        ContentId.newBuilder().setId(2).build(),
                        ContentId.newBuilder().setId(3).build()));
        Response response =
                mResponseBuilder.addRootFeature(ContentId.getDefaultInstance())
                        .addClusterFeature(
                                ContentId.getDefaultInstance(), ContentId.getDefaultInstance())
                        .addCard(ContentId.getDefaultInstance(), ContentId.getDefaultInstance())
                        .build();

        FeedProtocolAdapter mProtocolAdapter =
                new FeedProtocolAdapter(ImmutableList.of(mAdapter), mTimingUtils);
        Result<Model> result = mProtocolAdapter.createModel(response);

        verify(mAdapter, times(4)).determineRequiredContentIds(any(DataOperation.class));
        assertThat(result.isSuccessful()).isTrue();
        List<StreamDataOperation> operations = result.getValue().streamDataOperations;
        assertThat(operations).hasSize(7);
        assertThat(operations.get(4).getStreamStructure().getOperation())
                .isEqualTo(StreamStructure.Operation.REQUIRED_CONTENT);
        assertThat(operations.get(5).getStreamStructure().getOperation())
                .isEqualTo(StreamStructure.Operation.REQUIRED_CONTENT);
        assertThat(operations.get(6).getStreamStructure().getOperation())
                .isEqualTo(StreamStructure.Operation.REQUIRED_CONTENT);
        assertThat(ImmutableList.of(operations.get(4).getStreamStructure().getContentId(),
                           operations.get(5).getStreamStructure().getContentId(),
                           operations.get(6).getStreamStructure().getContentId()))
                .containsExactly("::::1", "::::2", "::::3");
    }
}
