// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Implementation of {@link UploadableActionMutation} */
public final class FeedUploadableActionMutation implements UploadableActionMutation {
    private static final String TAG = "FeedUploadableActionMutation";

    private final Map<String, FeedUploadableActionChanges> mActions = new HashMap<>();
    private final Committer<CommitResult, Map<String, FeedUploadableActionChanges>> mCommitter;

    FeedUploadableActionMutation(
            Committer<CommitResult, Map<String, FeedUploadableActionChanges>> committer) {
        this.mCommitter = committer;
    }

    @Override
    public UploadableActionMutation upsert(StreamUploadableAction action, String contentId) {
        @Nullable
        FeedUploadableActionChanges actionsForId = mActions.get(contentId);
        if (actionsForId == null) {
            actionsForId = new FeedUploadableActionChanges();
        }
        actionsForId.upsertAction(action);
        mActions.put(contentId, actionsForId);
        Logger.i(TAG, "Adding action %d", action);
        return this;
    }

    @Override
    public UploadableActionMutation remove(StreamUploadableAction action, String contentId) {
        @Nullable
        FeedUploadableActionChanges actionsForId = mActions.get(contentId);
        if (actionsForId == null) {
            actionsForId = new FeedUploadableActionChanges();
        }
        actionsForId.removeAction(action);
        mActions.put(contentId, actionsForId);
        Logger.i(TAG, "Removing action %d", action);
        return this;
    }

    @Override
    public CommitResult commit() {
        return mCommitter.commit(mActions);
    }

    public static class FeedUploadableActionChanges {
        private final Set<StreamUploadableAction> mUpsertActions;
        private final Set<StreamUploadableAction> mRemoveActions;

        FeedUploadableActionChanges() {
            this.mUpsertActions = new HashSet<>();
            this.mRemoveActions = new HashSet<>();
        }

        void upsertAction(StreamUploadableAction action) {
            mUpsertActions.add(action);
            mRemoveActions.remove(action);
        }

        void removeAction(StreamUploadableAction action) {
            mRemoveActions.add(action);
            mUpsertActions.remove(action);
        }

        public Set<StreamUploadableAction> upsertActions() {
            return mUpsertActions;
        }

        public Set<StreamUploadableAction> removeActions() {
            return mRemoveActions;
        }
    }
}
