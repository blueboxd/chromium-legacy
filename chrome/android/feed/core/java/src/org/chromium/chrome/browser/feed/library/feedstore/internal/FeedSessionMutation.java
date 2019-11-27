// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.ArrayList;
import java.util.List;

/** Implementation of the {@link SessionMutation}. */
public final class FeedSessionMutation implements SessionMutation {
    private final List<StreamStructure> mStreamStructures = new ArrayList<>();
    private final Committer<Boolean, List<StreamStructure>> mCommitter;

    FeedSessionMutation(Committer<Boolean, List<StreamStructure>> committer) {
        this.mCommitter = committer;
    }

    @Override
    public SessionMutation add(StreamStructure streamStructure) {
        mStreamStructures.add(streamStructure);
        return this;
    }

    @Override
    public Boolean commit() {
        return mCommitter.commit(mStreamStructures);
    }
}
