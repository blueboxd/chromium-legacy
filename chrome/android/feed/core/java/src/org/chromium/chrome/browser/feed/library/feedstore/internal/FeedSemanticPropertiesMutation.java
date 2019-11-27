// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;

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
