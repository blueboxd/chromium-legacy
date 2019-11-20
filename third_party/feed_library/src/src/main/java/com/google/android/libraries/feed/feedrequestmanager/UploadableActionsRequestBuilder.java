// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.feedrequestmanager;

import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.Result;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import com.google.search.now.wire.feed.ActionRequestProto.ActionRequest;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.FeedActionProto.FeedAction;
import com.google.search.now.wire.feed.FeedActionRequestProto.FeedActionRequest;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

// A class that creates an ActionsRequest for uploading actions
final class UploadableActionsRequestBuilder {
  private Set<StreamUploadableAction> uploadableActions;
  private ConsistencyToken token;
  private final ProtocolAdapter protocolAdapter;
  private final HashMap<String, byte[]> semanticPropertiesMap = new HashMap<>();

  UploadableActionsRequestBuilder(ProtocolAdapter protocolAdapter) {
    this.protocolAdapter = protocolAdapter;
  }

  UploadableActionsRequestBuilder setConsistencyToken(ConsistencyToken token) {
    this.token = token;
    return this;
  }

  boolean hasConsistencyToken() {
    return token != null;
  }

  UploadableActionsRequestBuilder setActions(Set<StreamUploadableAction> uploadableActions) {
    this.uploadableActions = uploadableActions;
    return this;
  }

  UploadableActionsRequestBuilder setSemanticProperties(
      List<SemanticPropertiesWithId> semanticPropertiesList) {
    for (SemanticPropertiesWithId semanticProperties : semanticPropertiesList) {
      semanticPropertiesMap.put(semanticProperties.contentId, semanticProperties.semanticData);
    }
    return this;
  }

  public ActionRequest build() {
    ActionRequest.Builder requestBuilder =
        ActionRequest.newBuilder()
            .setRequestVersion(ActionRequest.RequestVersion.FEED_UPLOAD_ACTION);
    FeedActionRequest.Builder feedActionRequestBuilder = FeedActionRequest.newBuilder();
    if (uploadableActions != null) {
      for (StreamUploadableAction action : uploadableActions) {
        String contentId = action.getFeatureContentId();
        ActionPayload payload = action.getPayload();
        FeedAction.Builder feedAction =
            FeedAction.newBuilder()
                .setActionPayload(payload)
                .setClientData(
                    FeedAction.ClientData.newBuilder()
                        .setTimestampSeconds(action.getTimestampSeconds())
                        .build());
        if (semanticPropertiesMap.containsKey(contentId)) {
          feedAction.setSemanticProperties(
              SemanticProperties.newBuilder()
                  .setSemanticPropertiesData(
                      ByteString.copyFrom(semanticPropertiesMap.get(contentId))));
        }
        Result<ContentId> contentIdResult = protocolAdapter.getWireContentId(contentId);
        if (contentIdResult.isSuccessful()) {
          feedAction.setContentId(contentIdResult.getValue());
        }
        feedActionRequestBuilder.addFeedAction(feedAction);
      }
    }
    if (hasConsistencyToken()) {
      feedActionRequestBuilder.setConsistencyToken(token);
    }
    requestBuilder.setExtension(
        FeedActionRequest.feedActionRequest, feedActionRequestBuilder.build());

    return requestBuilder.build();
  }
}
