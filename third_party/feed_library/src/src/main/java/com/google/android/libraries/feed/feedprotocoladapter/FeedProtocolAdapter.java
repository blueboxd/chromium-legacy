// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedprotocoladapter;

import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.protocoladapter.RequiredContentAdapter;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.Validators;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.android.libraries.feed.feedprotocoladapter.internal.transformers.ContentDataOperationTransformer;
import com.google.android.libraries.feed.feedprotocoladapter.internal.transformers.DataOperationTransformer;
import com.google.android.libraries.feed.feedprotocoladapter.internal.transformers.FeatureDataOperationTransformer;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.ContentIdProto.ContentId.Builder;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeatureProto.Feature;
import com.google.search.now.wire.feed.FeatureProto.Feature.RenderableUnit;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponse;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;
import com.google.search.now.wire.feed.PietSharedStateItemProto.PietSharedStateItem;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.TokenProto.Token;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A ProtocolAdapter which converts from the wire protocol to the internal protocol. */
public final class FeedProtocolAdapter implements ProtocolAdapter, Dumpable {
    private static final String TAG = "FeedProtocolAdapter";
    static final String CONTENT_ID_DELIMITER = "::";

    private final List<DataOperationTransformer> mDataOperationTransformers;
    private final List<RequiredContentAdapter> mRequiredContentAdapters;
    private final TimingUtils mTimingUtils;

    // Operation counts for #dump(Dumpable)
    private int mResponseHandlingCount;
    private int mConvertContentIdCount;

    public FeedProtocolAdapter(
            List<RequiredContentAdapter> requiredContentAdapters, TimingUtils timingUtils) {
        this.mTimingUtils = timingUtils;
        this.mRequiredContentAdapters = requiredContentAdapters;
        mDataOperationTransformers = new ArrayList<>(2);
        mDataOperationTransformers.add(new FeatureDataOperationTransformer());
        mDataOperationTransformers.add(new ContentDataOperationTransformer());
    }

    @Override
    public String getStreamContentId(ContentId contentId) {
        mConvertContentIdCount++;
        return createContentId(contentId);
    }

    @Override
    public Result<ContentId> getWireContentId(String contentId) {
        String[] splitContentId = contentId.split(CONTENT_ID_DELIMITER, -1);
        // Can't create if all 3 parts aren't present (at the very least empty)
        if (splitContentId.length != 3) {
            Logger.e(TAG, "Error parsing string content ID - splitting did not result in 3 parts");
            return Result.failure();
        }
        String table = splitContentId[0];
        String contentDomain = splitContentId[1];
        long id;
        try {
            id = Long.parseLong(splitContentId[2]);
        } catch (NumberFormatException e) {
            Logger.e(TAG, e, "Error converting content ID to wire format");
            return Result.failure();
        }
        Builder builder = ContentId.newBuilder().setId(id);
        if (!table.isEmpty()) {
            builder.setTable(table);
        }
        if (!contentDomain.isEmpty()) {
            builder.setContentDomain(contentDomain);
        }
        return Result.success(builder.build());
    }

    @Override
    public Result<Model> createModel(Response response) {
        mResponseHandlingCount++;

        FeedResponse feedResponse = response.getExtension(FeedResponse.feedResponse);
        Logger.i(TAG, "createModel, operations %s", feedResponse.getDataOperationCount());
        Result<List<StreamDataOperation>> result = createOperations(
                feedResponse.getDataOperationList(), feedResponse.getFeedResponseMetadata());
        if (result.isSuccessful()) {
            return Result.success(Model.of(result.getValue()));
        } else {
            return Result.failure();
        }
    }

    @Override
    public Result<List<StreamDataOperation>> createOperations(List<DataOperation> dataOperations) {
        return createOperations(dataOperations, FeedResponseMetadata.getDefaultInstance());
    }

    private Result<List<StreamDataOperation>> createOperations(
            List<DataOperation> dataOperations, FeedResponseMetadata responseMetadata) {
        ElapsedTimeTracker totalTimeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<StreamDataOperation> streamDataOperations = new ArrayList<>();
        Set<ContentId> requiredContentIds = new HashSet<>();
        for (DataOperation operation : dataOperations) {
            for (RequiredContentAdapter adapter : mRequiredContentAdapters) {
                requiredContentIds.addAll(adapter.determineRequiredContentIds(operation));
            }

            Operation streamOperation = operationToStreamOperation(operation.getOperation());
            String contentId;
            if (streamOperation == Operation.CLEAR_ALL) {
                streamDataOperations.add(
                        createDataOperation(Operation.CLEAR_ALL, null, null).build());
                continue;
            } else if (streamOperation == Operation.REMOVE) {
                if (operation.getMetadata().hasContentId()) {
                    contentId = createContentId(operation.getMetadata().getContentId());
                    String parentId = null;
                    if (operation.getFeature().hasParentId()) {
                        parentId = createContentId(operation.getFeature().getParentId());
                    }
                    streamDataOperations.add(
                            createDataOperation(Operation.REMOVE, contentId, parentId).build());

                } else {
                    Logger.w(TAG,
                            "REMOVE defined without a ContentId identifying the item to remove");
                }
                continue;
            } else if (operation.getMetadata().hasContentId()) {
                contentId = createContentId(operation.getMetadata().getContentId());
            } else {
                // This is an error state, every card should have a content id
                Logger.e(TAG, "ContentId not defined for DataOperation");
                continue;
            }

            if (operation.hasFeature()) {
                handleFeatureOperation(
                        operation, responseMetadata, contentId, streamDataOperations);
            } else if (operation.hasPietSharedState()) {
                PietSharedStateItem item =
                        PietSharedStateItem.newBuilder()
                                .setPietSharedState(operation.getPietSharedState())
                                .build();
                StreamSharedState state = StreamSharedState.newBuilder()
                                                  .setPietSharedStateItem(item)
                                                  .setContentId(contentId)
                                                  .build();
                streamDataOperations.add(
                        createSharedStateDataOperation(streamOperation, contentId, state).build());
            }

            if (operation.getMetadata().getSemanticProperties().hasSemanticPropertiesData()) {
                streamDataOperations.add(createSemanticDataOperation(contentId,
                        operation.getMetadata().getSemanticProperties().getSemanticPropertiesData())
                                                 .build());
            }
        }

        for (ContentId requiredContentId : requiredContentIds) {
            streamDataOperations.add(
                    StreamDataOperation.newBuilder()
                            .setStreamStructure(
                                    StreamStructure.newBuilder()
                                            .setOperation(Operation.REQUIRED_CONTENT)
                                            .setContentId(createContentId(requiredContentId)))
                            .build());
        }

        totalTimeTracker.stop("task", "convertWireProtocol", "mutations", dataOperations.size());
        return Result.success(streamDataOperations);
    }

    private void handleFeatureOperation(DataOperation operation,
            FeedResponseMetadata feedResponseMetadata, String contentId,
            List<StreamDataOperation> streamDataOperations) {
        Operation streamOperation = operationToStreamOperation(operation.getOperation());
        String parentId = null;
        if (operation.getFeature().hasParentId()) {
            parentId = createContentId(operation.getFeature().getParentId());
        }
        if (operation.getFeature().getRenderableUnit() == RenderableUnit.TOKEN) {
            Feature feature = operation.getFeature();
            if (feature.hasExtension(Token.tokenExtension)) {
                // Create a StreamToken operation
                Token token = feature.getExtension(Token.tokenExtension);
                StreamToken streamToken = Validators.checkNotNull(
                        createStreamToken(contentId, parentId, token.getNextPageToken()));
                streamDataOperations.add(
                        createTokenDataOperation(contentId, streamToken.getParentId(), streamToken)
                                .build());
            } else {
                Logger.e(TAG, "Extension not found for TOKEN");
            }
        } else {
            StreamFeature.Builder streamFeatureBuilder = createStreamFeature(contentId, parentId);
            StreamDataOperation.Builder streamDataOperation =
                    StreamDataOperation.newBuilder().setStreamPayload(
                            StreamPayload.newBuilder().setStreamFeature(streamFeatureBuilder));
            streamDataOperation =
                    transformOperation(operation, streamDataOperation, feedResponseMetadata);
            streamDataOperation =
                    createDataOperation(streamDataOperation, streamOperation, contentId, parentId);
            streamDataOperations.add(streamDataOperation.build());
        }
    }

    private StreamDataOperation.Builder transformOperation(DataOperation operation,
            StreamDataOperation.Builder streamDataOperation,
            FeedResponseMetadata feedResponseMetadata) {
        for (DataOperationTransformer transformer : mDataOperationTransformers) {
            streamDataOperation =
                    transformer.transform(operation, streamDataOperation, feedResponseMetadata);
        }

        return streamDataOperation;
    }

    private StreamFeature.Builder createStreamFeature(
            String contentId, /*@Nullable*/ String parentId) {
        StreamFeature.Builder builder = StreamFeature.newBuilder();
        builder.setContentId(contentId);

        if (parentId != null) {
            builder.setParentId(parentId);
        }
        return builder;
    }

    /*@Nullable*/
    private StreamToken createStreamToken(
            String tokenId, /*@Nullable*/ String parentId, ByteString continuationToken) {
        if (continuationToken.isEmpty()) {
            return null;
        }
        StreamToken.Builder tokenBuilder = StreamToken.newBuilder();
        if (parentId != null) {
            tokenBuilder.setParentId(parentId);
        }
        tokenBuilder.setContentId(tokenId);
        tokenBuilder.setNextPageToken(continuationToken);
        return tokenBuilder.build();
    }

    private StreamDataOperation.Builder createSharedStateDataOperation(
            Operation operation, /*@Nullable*/ String contentId, StreamSharedState sharedState) {
        StreamDataOperation.Builder dataOperation = createDataOperation(operation, contentId, null);
        dataOperation.setStreamPayload(
                StreamPayload.newBuilder().setStreamSharedState(sharedState));
        return dataOperation;
    }

    private StreamDataOperation.Builder createTokenDataOperation(
            /*@Nullable*/ String contentId, /*@Nullable*/ String parentId,
            StreamToken streamToken) {
        StreamDataOperation.Builder dataOperation =
                createDataOperation(Operation.UPDATE_OR_APPEND, contentId, parentId);
        dataOperation.setStreamPayload(StreamPayload.newBuilder().setStreamToken(streamToken));
        return dataOperation;
    }

    private StreamDataOperation.Builder createDataOperation(
            Operation operation, /*@Nullable*/ String contentId, /*@Nullable*/ String parentId) {
        return createDataOperation(
                StreamDataOperation.newBuilder(), operation, contentId, parentId);
    }

    private StreamDataOperation.Builder createDataOperation(
            StreamDataOperation.Builder streamDataOperation, Operation operation,
            /*@Nullable*/ String contentId,
            /*@Nullable*/ String parentId) {
        StreamStructure.Builder streamStructure = StreamStructure.newBuilder();
        streamStructure.setOperation(operation);
        if (contentId != null) {
            streamStructure.setContentId(contentId);
        }
        if (parentId != null) {
            streamStructure.setParentContentId(parentId);
        }
        streamDataOperation.setStreamStructure(streamStructure);
        return streamDataOperation;
    }

    private StreamDataOperation.Builder createSemanticDataOperation(
            String contentId, ByteString semanticData) {
        return StreamDataOperation.newBuilder()
                .setStreamPayload(StreamPayload.newBuilder().setSemanticData(semanticData))
                .setStreamStructure(
                        StreamStructure.newBuilder().setContentId(contentId).setOperation(
                                Operation.UPDATE_OR_APPEND));
    }

    private String createContentId(ContentId contentId) {
        // Using String concat for performance reasons.  This is called a lot for large feed
        // responses.
        return contentId.getTable() + CONTENT_ID_DELIMITER + contentId.getContentDomain()
                + CONTENT_ID_DELIMITER + contentId.getId();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("responseHandlingCount").value(mResponseHandlingCount);
        dumper.forKey("convertContentIdCount").value(mConvertContentIdCount).compactPrevious();
    }

    private static Operation operationToStreamOperation(DataOperation.Operation operation) {
        switch (operation) {
            case UNKNOWN_OPERATION:
                return Operation.UNKNOWN;
            case CLEAR_ALL:
                return Operation.CLEAR_ALL;
            case UPDATE_OR_APPEND:
                return Operation.UPDATE_OR_APPEND;
            case REMOVE:
                return Operation.REMOVE;
        }

        return Operation.UNKNOWN;
    }
}
