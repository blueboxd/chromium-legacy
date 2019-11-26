// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
