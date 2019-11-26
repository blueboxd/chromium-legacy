// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.store.SemanticPropertiesMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.protobuf.ByteString;

import java.util.HashMap;
import java.util.Map;

/** Implementation of the {@link SemanticPropertiesMutation}. */
public final class FeedSemanticPropertiesMutation implements SemanticPropertiesMutation {
    private final Map<String, ByteString> mSemanticPropertiesMap = new HashMap<>();
    private final Committer<CommitResult, Map<String, ByteString>> mCommitter;

    FeedSemanticPropertiesMutation(Committer<CommitResult, Map<String, ByteString>> committer) {
        this.mCommitter = committer;
    }

    @Override
    public SemanticPropertiesMutation add(String contentId, ByteString semanticData) {
        mSemanticPropertiesMap.put(contentId, semanticData);
        return this;
    }

    @Override
    public CommitResult commit() {
        return mCommitter.commit(mSemanticPropertiesMap);
    }
}
