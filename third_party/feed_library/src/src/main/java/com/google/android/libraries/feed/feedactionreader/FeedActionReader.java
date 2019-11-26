// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedactionreader;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionReader;
import com.google.android.libraries.feed.api.internal.common.DismissActionWithSemanticProperties;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Feed implementation of {@link ActionReader} */
public final class FeedActionReader implements ActionReader {
    private static final String TAG = "FeedActionReader";

    private final Store mStore;
    private final Clock mClock;
    private final ProtocolAdapter mProtocolAdapter;
    private final TaskQueue mTaskQueue;
    private final long mDismissActionTTLSeconds;
    private final double mMinValidActionRatio;

    public FeedActionReader(Store store, Clock clock, ProtocolAdapter protocolAdapter,
            TaskQueue taskQueue, Configuration configuration) {
        this.mStore = store;
        this.mClock = clock;
        this.mProtocolAdapter = protocolAdapter;
        this.mTaskQueue = taskQueue;
        this.mDismissActionTTLSeconds = configuration.getValueOrDefault(
                ConfigKey.DEFAULT_ACTION_TTL_SECONDS, TimeUnit.DAYS.toSeconds(3L));
        mMinValidActionRatio =
                configuration.getValueOrDefault(ConfigKey.MINIMUM_VALID_ACTION_RATIO, 0.3);
    }

    @Override
    public Result<List<DismissActionWithSemanticProperties>>
    getDismissActionsWithSemanticProperties() {
        Result<List<StreamLocalAction>> dismissActionsResult = mStore.getAllDismissLocalActions();
        if (!dismissActionsResult.isSuccessful()) {
            Logger.e(TAG, "Error fetching dismiss actions from store");
            return Result.failure();
        }
        List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();
        Set<String> contentIds = new HashSet<>(dismissActions.size());
        long minValidTime = TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis())
                - mDismissActionTTLSeconds;
        for (StreamLocalAction dismissAction : dismissActions) {
            if (dismissAction.getTimestampSeconds() > minValidTime) {
                contentIds.add(dismissAction.getFeatureContentId());
            }
        }

        // Clean up if necessary
        // Note that since we're using a Set, it's possible this will trigger prematurely due to
        // duplicates not being counted as valid.
        if ((double) contentIds.size() / dismissActions.size() < mMinValidActionRatio) {
            mTaskQueue.execute(Task.LOCAL_ACTION_GC, TaskType.BACKGROUND,
                    mStore.triggerLocalActionGc(dismissActions, new ArrayList<>(contentIds)));
        }

        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                mStore.getSemanticProperties(new ArrayList<>(contentIds));
        if (!semanticPropertiesResult.isSuccessful()) {
            return Result.failure();
        }
        List<DismissActionWithSemanticProperties> dismissActionWithSemanticProperties =
                new ArrayList<>(contentIds.size());

        for (SemanticPropertiesWithId semanticPropertiesWithId :
                semanticPropertiesResult.getValue()) {
            Result<ContentId> wireContentIdResult =
                    mProtocolAdapter.getWireContentId(semanticPropertiesWithId.contentId);
            if (!wireContentIdResult.isSuccessful()) {
                Logger.e(TAG, "Error converting to wire result for contentId: %s",
                        semanticPropertiesWithId.contentId);
                continue;
            }
            dismissActionWithSemanticProperties.add(new DismissActionWithSemanticProperties(
                    wireContentIdResult.getValue(), semanticPropertiesWithId.semanticData));
            // Also strip out from the content ids list (so that we can put those in with null
            // semantic properties
            contentIds.remove(semanticPropertiesWithId.contentId);
        }
        for (String contentId : contentIds) {
            Result<ContentId> wireContentIdResult = mProtocolAdapter.getWireContentId(contentId);
            if (!wireContentIdResult.isSuccessful()) {
                Logger.e(TAG, "Error converting to wire result for contentId: %s", contentId);
                continue;
            }
            dismissActionWithSemanticProperties.add(
                    new DismissActionWithSemanticProperties(wireContentIdResult.getValue(), null));
        }
        return Result.success(dismissActionWithSemanticProperties);
    }
}
