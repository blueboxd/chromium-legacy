// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.feed.core.proto.wire.ActionRequestProto.ActionRequest;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.wire.FeedActionRequestProto.FeedActionRequest;
import org.chromium.components.feed.core.proto.wire.SemanticPropertiesProto.SemanticProperties;

import java.util.HashMap;
import java.util.List;
import java.util.Set;

// A class that creates an ActionsRequest for uploading actions
final class UploadableActionsRequestBuilder {
    private Set<StreamUploadableAction> mUploadableActions;
    private ConsistencyToken mToken;
    private final ProtocolAdapter mProtocolAdapter;
    private final HashMap<String, byte[]> mSemanticPropertiesMap = new HashMap<>();

    UploadableActionsRequestBuilder(ProtocolAdapter protocolAdapter) {
        this.mProtocolAdapter = protocolAdapter;
    }

    UploadableActionsRequestBuilder setConsistencyToken(ConsistencyToken token) {
        this.mToken = token;
        return this;
    }

    boolean hasConsistencyToken() {
        return mToken != null;
    }

    UploadableActionsRequestBuilder setActions(Set<StreamUploadableAction> uploadableActions) {
        this.mUploadableActions = uploadableActions;
        return this;
    }

    UploadableActionsRequestBuilder setSemanticProperties(
            List<SemanticPropertiesWithId> semanticPropertiesList) {
        for (SemanticPropertiesWithId semanticProperties : semanticPropertiesList) {
            mSemanticPropertiesMap.put(
                    semanticProperties.contentId, semanticProperties.semanticData);
        }
        return this;
    }

    public ActionRequest build() {
        ActionRequest.Builder requestBuilder = ActionRequest.newBuilder().setRequestVersion(
                ActionRequest.RequestVersion.FEED_UPLOAD_ACTION);
        FeedActionRequest.Builder feedActionRequestBuilder = FeedActionRequest.newBuilder();
        if (mUploadableActions != null) {
            for (StreamUploadableAction action : mUploadableActions) {
                String contentId = action.getFeatureContentId();
                ActionPayload payload = action.getPayload();
                FeedAction.Builder feedAction =
                        FeedAction.newBuilder().setActionPayload(payload).setClientData(
                                FeedAction.ClientData.newBuilder()
                                        .setTimestampSeconds(action.getTimestampSeconds())
                                        .build());
                if (mSemanticPropertiesMap.containsKey(contentId)) {
                    feedAction.setSemanticProperties(
                            SemanticProperties.newBuilder().setSemanticPropertiesData(
                                    ByteString.copyFrom(mSemanticPropertiesMap.get(contentId))));
                }
                Result<ContentId> contentIdResult = mProtocolAdapter.getWireContentId(contentId);
                if (contentIdResult.isSuccessful()) {
                    feedAction.setContentId(contentIdResult.getValue());
                }
                feedActionRequestBuilder.addFeedAction(feedAction);
            }
        }
        if (hasConsistencyToken()) {
            feedActionRequestBuilder.setConsistencyToken(mToken);
        }
        requestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, feedActionRequestBuilder.build());

        return requestBuilder.build();
    }
}
