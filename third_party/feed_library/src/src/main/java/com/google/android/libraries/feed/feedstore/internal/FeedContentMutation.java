// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

import java.util.ArrayList;
import java.util.List;

/** This class will mutate the Content stored in the FeedStore. */
public final class FeedContentMutation implements ContentMutation {
    private final List<PayloadWithId> mMutations = new ArrayList<>();
    private final Committer<CommitResult, List<PayloadWithId>> mCommitter;

    FeedContentMutation(Committer<CommitResult, List<PayloadWithId>> committer) {
        this.mCommitter = committer;
    }

    @Override
    public ContentMutation add(String contentId, StreamPayload payload) {
        mMutations.add(new PayloadWithId(contentId, payload));
        return this;
    }

    @Override
    public CommitResult commit() {
        return mCommitter.commit(mMutations);
    }
}
