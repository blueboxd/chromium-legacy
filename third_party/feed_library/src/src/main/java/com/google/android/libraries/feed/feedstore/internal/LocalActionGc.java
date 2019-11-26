// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.JournalMutation.Builder;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;

import java.util.ArrayList;
import java.util.List;

/** Garbage collector for {@link StreamLocalAction}s stored in a journal */
public final class LocalActionGc {
    private static final String TAG = "LocalActionGc";

    private final List<StreamLocalAction> mActions;
    private final List<String> mValidContentIds;
    private final JournalStorageDirect mJournalStorageDirect;
    private final TimingUtils mTimingUtils;
    private final String mJournalName;

    LocalActionGc(List<StreamLocalAction> actions, List<String> validContentIds,
            JournalStorageDirect journalStorageDirect, TimingUtils timingUtils,
            String journalName) {
        this.mActions = actions;
        this.mValidContentIds = validContentIds;
        this.mJournalStorageDirect = journalStorageDirect;
        this.mTimingUtils = timingUtils;
        this.mJournalName = journalName;
    }

    /**
     * Cleans up the store based on {@link #mActions} and {@link #mValidContentIds}. Any valid
     * actions will be copied over to a new copy of the action journal.
     */
    void gc() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<StreamLocalAction> validActions = new ArrayList<>(mValidContentIds.size());

        for (StreamLocalAction action : mActions) {
            if (mValidContentIds.contains(action.getFeatureContentId())) {
                validActions.add(action);
            }
        }

        Builder mutationBuilder = new Builder(mJournalName);
        mutationBuilder.delete();

        for (StreamLocalAction action : validActions) {
            mutationBuilder.append(action.toByteArray());
        }
        CommitResult result = mJournalStorageDirect.commit(mutationBuilder.build());
        if (result == CommitResult.SUCCESS) {
            tracker.stop("gcMutation", mActions.size() - validActions.size());
        } else {
            tracker.stop("gcMutation failed");
        }
    }
}
