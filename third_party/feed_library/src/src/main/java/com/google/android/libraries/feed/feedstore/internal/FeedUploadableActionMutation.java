// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Implementation of {@link UploadableActionMutation} */
public final class FeedUploadableActionMutation implements UploadableActionMutation {
    private static final String TAG = "FeedUploadableActionMutation";

    private final Map<String, FeedUploadableActionChanges> actions = new HashMap<>();
    private final Committer<CommitResult, Map<String, FeedUploadableActionChanges>> committer;

    FeedUploadableActionMutation(
            Committer<CommitResult, Map<String, FeedUploadableActionChanges>> committer) {
        this.committer = committer;
    }

    @Override
    public UploadableActionMutation upsert(StreamUploadableAction action, String contentId) {
        /*@Nullable*/ FeedUploadableActionChanges actionsForId = actions.get(contentId);
        if (actionsForId == null) {
            actionsForId = new FeedUploadableActionChanges();
        }
        actionsForId.upsertAction(action);
        actions.put(contentId, actionsForId);
        Logger.i(TAG, "Added action %d", action);
        return this;
    }

    @Override
    public UploadableActionMutation remove(StreamUploadableAction action, String contentId) {
        /*@Nullable*/ FeedUploadableActionChanges actionsForId = actions.get(contentId);
        if (actionsForId == null) {
            actionsForId = new FeedUploadableActionChanges();
        }
        actionsForId.removeAction(action);
        actions.put(contentId, actionsForId);
        Logger.i(TAG, "Added action %d", action);
        return this;
    }

    @Override
    public CommitResult commit() {
        return committer.commit(actions);
    }

    public static class FeedUploadableActionChanges {
        private final Set<StreamUploadableAction> upsertActions;
        private final Set<StreamUploadableAction> removeActions;

        FeedUploadableActionChanges() {
            this.upsertActions = new HashSet<>();
            this.removeActions = new HashSet<>();
        }

        void upsertAction(StreamUploadableAction action) {
            upsertActions.add(action);
            removeActions.remove(action);
        }

        void removeAction(StreamUploadableAction action) {
            removeActions.add(action);
            upsertActions.remove(action);
        }

        public Set<StreamUploadableAction> upsertActions() {
            return upsertActions;
        }

        public Set<StreamUploadableAction> removeActions() {
            return removeActions;
        }
    }
}
