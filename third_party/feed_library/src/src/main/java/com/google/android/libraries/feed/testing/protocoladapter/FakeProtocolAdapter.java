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

package com.google.android.libraries.feed.testing.protocoladapter;

import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.Result;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.ResponseProto.Response;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Converts the wire protocol (protos sent from the server) into an internal representation. */
public final class FakeProtocolAdapter implements ProtocolAdapter {
  private static final String UNMAPPED_CONTENT_ID = "unmapped_content_id";

  private final Map<String, ContentId> contentIds = new HashMap<>();
  /*@Nullable*/ private Response lastResponse = null;

  @Override
  public Result<Model> createModel(Response response) {
    lastResponse = response;
    return Result.success(Model.empty());
  }

  @Override
  public Result<List<StreamDataOperation>> createOperations(List<DataOperation> dataOperations) {
    return Result.success(new ArrayList<>());
  }

  @Override
  public String getStreamContentId(ContentId wireContentId) {
    for (String contentId : contentIds.keySet()) {
      if (wireContentId.equals(contentIds.get(contentId))) {
        return contentId;
      }
    }

    return UNMAPPED_CONTENT_ID;
  }

  @Override
  public Result<ContentId> getWireContentId(String contentId) {
    ContentId wireContentId = contentIds.get(contentId);
    if (wireContentId != null) {
      return Result.success(wireContentId);
    } else {
      return Result.success(ContentId.getDefaultInstance());
    }
  }

  /** Adds a content id mapping to the {@link FakeProtocolAdapter}. */
  public FakeProtocolAdapter addContentId(String contentId, ContentId wireContentId) {
    contentIds.put(contentId, wireContentId);
    return this;
  }

  /** Returns the last response sent into {@link #createModel(Response)}. */
  /*@Nullable*/
  public Response getLastResponse() {
    return lastResponse;
  }
}
