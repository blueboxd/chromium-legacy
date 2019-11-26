// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

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
