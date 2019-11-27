// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common.testing;

import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

import java.util.ArrayList;
import java.util.List;

/** This is a builder class for creating internal protocol elements. */
public class InternalProtocolBuilder {
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final List<StreamDataOperation> mDataOperations = new ArrayList<>();

    /** This adds a Root Feature into the Stream of data operations */
    public InternalProtocolBuilder addRootFeature() {
        StreamFeature streamFeature = StreamFeature.newBuilder()
                                              .setContentId(mIdGenerators.createRootContentId(0))
                                              .build();
        StreamStructure streamStructure =
                StreamStructure.newBuilder()
                        .setOperation(Operation.UPDATE_OR_APPEND)
                        .setContentId(mIdGenerators.createRootContentId(0))
                        .build();
        StreamPayload streamPayload =
                StreamPayload.newBuilder().setStreamFeature(streamFeature).build();
        mDataOperations.add(StreamDataOperation.newBuilder()
                                    .setStreamStructure(streamStructure)
                                    .setStreamPayload(streamPayload)
                                    .build());
        return this;
    }

    public InternalProtocolBuilder addClearOperation() {
        StreamStructure streamStructure =
                StreamStructure.newBuilder().setOperation(Operation.CLEAR_ALL).build();
        mDataOperations.add(
                StreamDataOperation.newBuilder().setStreamStructure(streamStructure).build());
        return this;
    }

    public InternalProtocolBuilder addSharedState(String contentId) {
        StreamSharedState streamSharedState =
                StreamSharedState.newBuilder().setContentId(contentId).build();
        StreamStructure streamStructure = StreamStructure.newBuilder()
                                                  .setOperation(Operation.UPDATE_OR_APPEND)
                                                  .setContentId(contentId)
                                                  .build();
        StreamPayload streamPayload =
                StreamPayload.newBuilder().setStreamSharedState(streamSharedState).build();
        mDataOperations.add(StreamDataOperation.newBuilder()
                                    .setStreamStructure(streamStructure)
                                    .setStreamPayload(streamPayload)
                                    .build());
        return this;
    }

    public InternalProtocolBuilder addToken(String contentId) {
        StreamToken streamToken = StreamToken.newBuilder().setContentId(contentId).build();
        StreamStructure streamStructure = StreamStructure.newBuilder()
                                                  .setOperation(Operation.UPDATE_OR_APPEND)
                                                  .setContentId(contentId)
                                                  .build();
        StreamPayload streamPayload =
                StreamPayload.newBuilder().setStreamToken(streamToken).build();
        mDataOperations.add(StreamDataOperation.newBuilder()
                                    .setStreamStructure(streamStructure)
                                    .setStreamPayload(streamPayload)
                                    .build());
        return this;
    }

    public InternalProtocolBuilder addFeature(String contentId, String parentId) {
        StreamFeature streamFeature =
                StreamFeature.newBuilder().setContentId(contentId).setParentId(parentId).build();
        StreamStructure streamStructure = StreamStructure.newBuilder()
                                                  .setOperation(Operation.UPDATE_OR_APPEND)
                                                  .setContentId(contentId)
                                                  .setParentContentId(parentId)
                                                  .build();
        StreamPayload streamPayload =
                StreamPayload.newBuilder().setStreamFeature(streamFeature).build();
        mDataOperations.add(StreamDataOperation.newBuilder()
                                    .setStreamStructure(streamStructure)
                                    .setStreamPayload(streamPayload)
                                    .build());
        return this;
    }

    public InternalProtocolBuilder removeFeature(String contentId, String parentId) {
        StreamStructure streamStructure = StreamStructure.newBuilder()
                                                  .setOperation(Operation.REMOVE)
                                                  .setContentId(contentId)
                                                  .setParentContentId(parentId)
                                                  .build();
        mDataOperations.add(
                StreamDataOperation.newBuilder().setStreamStructure(streamStructure).build());
        return this;
    }

    public InternalProtocolBuilder addRequiredContent(String contentId) {
        mDataOperations.add(
                StreamDataOperation.newBuilder()
                        .setStreamStructure(StreamStructure.newBuilder()
                                                    .setOperation(Operation.REQUIRED_CONTENT)
                                                    .setContentId(contentId))
                        .build());
        return this;
    }

    public List<StreamDataOperation> build() {
        return new ArrayList<>(mDataOperations);
    }

    /**
     * This will return the StreamStructure part only, filtering out any shared state. This is the
     * form structure is represented in the Store.
     */
    public List<StreamStructure> buildAsStreamStructure() {
        List<StreamStructure> streamStructures = new ArrayList<>();
        for (StreamDataOperation operation : mDataOperations) {
            // For the structure, ignore the shared state
            if (operation.getStreamPayload().hasStreamSharedState()) {
                continue;
            }
            streamStructures.add(operation.getStreamStructure());
        }
        return streamStructures;
    }

    public List<PayloadWithId> buildAsPayloadWithId() {
        List<PayloadWithId> payloads = new ArrayList<>();
        for (StreamDataOperation operation : mDataOperations) {
            // For the structure, ignore the shared state
            if (operation.getStreamPayload().hasStreamSharedState()) {
                continue;
            }
            // Only include payloads with UPDATE_OR_APPEND.
            if (operation.getStreamStructure().getOperation() != Operation.UPDATE_OR_APPEND) {
                continue;
            }
            payloads.add(new PayloadWithId(
                    operation.getStreamStructure().getContentId(), operation.getStreamPayload()));
        }
        return payloads;
    }
}
