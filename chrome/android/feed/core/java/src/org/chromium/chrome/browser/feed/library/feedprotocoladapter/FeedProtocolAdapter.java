// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedprotocoladapter;

import androidx.annotation.Nullable;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.RequiredContentAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers.ContentDataOperationTransformer;
import org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers.DataOperationTransformer;
import org.chromium.chrome.browser.feed.library.feedprotocoladapter.internal.transformers.FeatureDataOperationTransformer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature.RenderableUnit;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponse;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponseMetadata;
import org.chromium.components.feed.core.proto.wire.PietSharedStateItemProto.PietSharedStateItem;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.TokenProto.Token;

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
        ContentId.Builder builder = ContentId.newBuilder().setId(id);
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
        List<StreamDataOperation> operations = createOperations(
                feedResponse.getDataOperationList(), feedResponse.getFeedResponseMetadata());
        return Result.success(Model.of(operations));
    }

    @Override
    public List<StreamDataOperation> createOperations(List<DataOperation> dataOperations) {
        return createOperations(dataOperations, FeedResponseMetadata.getDefaultInstance());
    }

    private List<StreamDataOperation> createOperations(
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
        return streamDataOperations;
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

    private StreamFeature.Builder createStreamFeature(String contentId, @Nullable String parentId) {
        StreamFeature.Builder builder = StreamFeature.newBuilder();
        builder.setContentId(contentId);

        if (parentId != null) {
            builder.setParentId(parentId);
        }
        return builder;
    }

    @Nullable
    private StreamToken createStreamToken(
            String tokenId, @Nullable String parentId, ByteString continuationToken) {
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
            Operation operation, @Nullable String contentId, StreamSharedState sharedState) {
        StreamDataOperation.Builder dataOperation = createDataOperation(operation, contentId, null);
        dataOperation.setStreamPayload(
                StreamPayload.newBuilder().setStreamSharedState(sharedState));
        return dataOperation;
    }

    private StreamDataOperation.Builder createTokenDataOperation(
            @Nullable String contentId, @Nullable String parentId, StreamToken streamToken) {
        StreamDataOperation.Builder dataOperation =
                createDataOperation(Operation.UPDATE_OR_APPEND, contentId, parentId);
        dataOperation.setStreamPayload(StreamPayload.newBuilder().setStreamToken(streamToken));
        return dataOperation;
    }

    private StreamDataOperation.Builder createDataOperation(
            Operation operation, @Nullable String contentId, @Nullable String parentId) {
        return createDataOperation(
                StreamDataOperation.newBuilder(), operation, contentId, parentId);
    }

    private StreamDataOperation.Builder createDataOperation(
            StreamDataOperation.Builder streamDataOperation, Operation operation,
            @Nullable String contentId, @Nullable String parentId) {
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
