// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.protocoladapter;

import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Converts the wire protocol (protos sent from the server) into an internal representation. */
public final class FakeProtocolAdapter implements ProtocolAdapter {
    private static final String UNMAPPED_CONTENT_ID = "unmapped_content_id";

    private final Map<String, ContentId> mContentIds = new HashMap<>();
    /*@Nullable*/ private Response mLastResponse;

    @Override
    public Result<Model> createModel(Response response) {
        mLastResponse = response;
        return Result.success(Model.empty());
    }

    @Override
    public Result<List<StreamDataOperation>> createOperations(List<DataOperation> dataOperations) {
        return Result.success(new ArrayList<>());
    }

    @Override
    public String getStreamContentId(ContentId wireContentId) {
        for (String contentId : mContentIds.keySet()) {
            if (wireContentId.equals(mContentIds.get(contentId))) {
                return contentId;
            }
        }

        return UNMAPPED_CONTENT_ID;
    }

    @Override
    public Result<ContentId> getWireContentId(String contentId) {
        ContentId wireContentId = mContentIds.get(contentId);
        if (wireContentId != null) {
            return Result.success(wireContentId);
        } else {
            return Result.success(ContentId.getDefaultInstance());
        }
    }

    /** Adds a content id mapping to the {@link FakeProtocolAdapter}. */
    public FakeProtocolAdapter addContentId(String contentId, ContentId wireContentId) {
        mContentIds.put(contentId, wireContentId);
        return this;
    }

    /** Returns the last response sent into {@link #createModel(Response)}. */
    /*@Nullable*/
    public Response getLastResponse() {
        return mLastResponse;
    }
}
