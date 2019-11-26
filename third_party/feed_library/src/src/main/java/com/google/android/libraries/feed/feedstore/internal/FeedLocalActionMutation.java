// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.android.libraries.feed.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Implementation of {@link LocalActionMutation} */
public final class FeedLocalActionMutation implements LocalActionMutation {
    private static final String TAG = "FeedLocalActionMutation";

    private final Map<Integer, List<String>> mActions = new HashMap<>();
    private final Committer<CommitResult, Map<Integer, List<String>>> mCommitter;

    FeedLocalActionMutation(Committer<CommitResult, Map<Integer, List<String>>> committer) {
        this.mCommitter = committer;
    }

    @Override
    public LocalActionMutation add(int action, String contentId) {
        /*@Nullable*/ List<String> actionsForType = mActions.get(action);
        if (actionsForType == null) {
            actionsForType = new ArrayList<>();
        }
        actionsForType.add(contentId);
        mActions.put(action, actionsForType);
        Logger.i(TAG, "Added action %d with content id %s", action, contentId);
        return this;
    }

    @Override
    public CommitResult commit() {
        return mCommitter.commit(mActions);
    }
}
