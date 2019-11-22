// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.protocoladapter;

import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.common.Result;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.ResponseProto.Response;

import java.util.List;

/** Converts the wire protocol (protos sent from the server) into an internal representation. */
public interface ProtocolAdapter {
    /**
     * Create the internal protocol from a wire protocol response definition. The wire protocol is
     * turned into a {@link Model} containing a list of {@link StreamDataOperation}s and a schema
     * version.
     */
    Result<Model> createModel(Response response);

    /**
     * Create {@link StreamDataOperation}s from the internal protocol for the wire protocol
     * DataOperations.
     */
    Result<List<StreamDataOperation>> createOperations(List<DataOperation> dataOperations);

    /**
     * Convert a wire protocol ContentId into the {@code String} version. Inverse of {@link
     * #getWireContentId(String)}
     */
    String getStreamContentId(ContentId contentId);

    /**
     * Convert a string ContentId into the wire protocol version. Inverse of {@link
     * #getStreamContentId(ContentId)}. Note that due to default proto values, if no ID was set in
     * {@link #getStreamContentId(ContentId)}, this method will set the ID to 0.
     */
    Result<ContentId> getWireContentId(String contentId);
}
