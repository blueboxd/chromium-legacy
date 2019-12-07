// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

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
        @Nullable
        List<String> actionsForType = mActions.get(action);
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
