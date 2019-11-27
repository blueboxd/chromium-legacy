// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;

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
