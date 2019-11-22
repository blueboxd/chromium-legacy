// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedprotocoladapter.internal.transformers;

import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;

/**
 * A DataOperationTransformer transform a {@link DataOperation} into a {@link
 * StreamDataOperation.Builder}. DataOperationTranformer(s) can be chained to perform multiple
 * transformations.
 */
public interface DataOperationTransformer {
    /**
     * Transforms a {@link DataOperation} into a {@link StreamDataOperation.Builder}. {@link
     * StreamDataOperation.Builder} is returned to allow for multiple transformations.
     */
    StreamDataOperation.Builder transform(DataOperation dataOperation,
            StreamDataOperation.Builder streamDataOperation,
            FeedResponseMetadata feedResponseMetadata);
}
