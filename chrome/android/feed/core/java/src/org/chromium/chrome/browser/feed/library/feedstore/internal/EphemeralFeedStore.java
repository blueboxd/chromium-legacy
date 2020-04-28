// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import com.google.protobuf.ByteString;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Ephemeral version of Store */
public final class EphemeralFeedStore implements ClearableStore {
    private static final String TAG = "EphemeralFeedStore";
    private static final Runnable EMPTY_RUNNABLE = () -> {};

    private final Clock mClock;
    private final TimingUtils mTimingUtils;
    private final FeedStoreHelper mStoreHelper;

    private final Map<String, PayloadWithId> mPayloadWithIdMap = new HashMap<>();
    private final Map<String, StreamSharedState> mSharedStateMap = new HashMap<>();
    private final Map<String, ByteString> mSemanticPropertiesMap = new HashMap<>();
    private final Map<Integer, List<StreamLocalAction>> mActionsMap = new HashMap<>();
    private final Map<String, Set<StreamUploadableAction>> mUploadableActionsMap = new HashMap<>();
    private final Map<String, List<StreamStructure>> mSessionsMap = new HashMap<>();

    public EphemeralFeedStore(Clock clock, TimingUtils timingUtils, FeedStoreHelper storeHelper) {
        this.mClock = clock;
        this.mTimingUtils = timingUtils;
        this.mStoreHelper = storeHelper;
    }

    @Override
    public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
        List<PayloadWithId> payloads = new ArrayList<>(contentIds.size());
        for (String contentId : contentIds) {
            PayloadWithId payload = mPayloadWithIdMap.get(contentId);
            if (payload != null) {
                payloads.add(payload);
            }
        }
        return Result.success(payloads);
    }

    @Override
    public Result<List<StreamSharedState>> getSharedStates() {
        return Result.success(
                Collections.unmodifiableList(new ArrayList<>(mSharedStateMap.values())));
    }

    @Override
    public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
        List<StreamStructure> streamStructures = mSessionsMap.get(sessionId);
        if (streamStructures == null) {
            streamStructures = Collections.emptyList();
        }
        return Result.success(streamStructures);
    }

    @Override
    public Result<List<String>> getAllSessions() {
        Set<String> sessions = mSessionsMap.keySet();
        ArrayList<String> returnValues = new ArrayList<>();
        for (String sessionId : sessions) {
            if (!HEAD_SESSION_ID.equals(sessionId)) {
                returnValues.add(sessionId);
            }
        }
        return Result.success(returnValues);
    }

    @Override
    public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
        List<SemanticPropertiesWithId> semanticPropertiesWithIds =
                new ArrayList<>(contentIds.size());
        for (String contentId : contentIds) {
            ByteString semanticProperties = mSemanticPropertiesMap.get(contentId);
            if (semanticProperties != null) {
                // TODO: switch SemanticPropertiesWithId to use byte array directly
                semanticPropertiesWithIds.add(
                        new SemanticPropertiesWithId(contentId, semanticProperties.toByteArray()));
            }
        }
        return Result.success(semanticPropertiesWithIds);
    }

    @Override
    public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
        List<StreamLocalAction> dismissActions = mActionsMap.get(ActionType.DISMISS);
        if (dismissActions == null) {
            dismissActions = Collections.emptyList();
        }
        return Result.success(dismissActions);
    }

    @Override
    public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
        Set<StreamUploadableAction> uploadableActions = new HashSet<>();
        for (Set<StreamUploadableAction> actions : mUploadableActionsMap.values()) {
            uploadableActions.addAll(actions);
        }
        return Result.success(uploadableActions);
    }

    @Override
    public Result<String> createNewSession() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        String sessionId = mStoreHelper.getNewStreamSessionId();
        Result<List<StreamStructure>> streamStructuresResult = getStreamStructures(HEAD_SESSION_ID);
        mSessionsMap.put(sessionId, new ArrayList<>(streamStructuresResult.getValue()));
        tracker.stop("createNewSession", sessionId);
        return Result.success(sessionId);
    }

    @Override
    public void removeSession(String sessionId) {
        if (sessionId.equals(HEAD_SESSION_ID)) {
            throw new IllegalStateException("Unable to delete the $HEAD session");
        }
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        mSessionsMap.remove(sessionId);
        tracker.stop("removeSession", sessionId);
    }

    @Override
    public void clearHead() {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        mSessionsMap.remove(HEAD_SESSION_ID);
        tracker.stop("", "clearHead");
    }

    @Override
    public ContentMutation editContent() {
        return new FeedContentMutation(this::commitContentMutation);
    }

    private CommitResult commitContentMutation(List<PayloadWithId> mutations) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);

        for (PayloadWithId mutation : mutations) {
            String contentId = mutation.contentId;
            if (mutation.payload.hasStreamSharedState()) {
                StreamSharedState streamSharedState = mutation.payload.getStreamSharedState();
                mSharedStateMap.put(contentId, streamSharedState);
            } else {
                mPayloadWithIdMap.put(contentId, mutation);
            }
        }
        tracker.stop("task", "commitContentMutation", "mutations", mutations.size());
        return CommitResult.SUCCESS;
    }

    @Override
    public SessionMutation editSession(String sessionId) {
        return new FeedSessionMutation(
                feedSessionMutation -> commitSessionMutation(sessionId, feedSessionMutation));
    }

    private Boolean commitSessionMutation(
            String sessionId, List<StreamStructure> streamStructures) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<StreamStructure> sessionStructures = mSessionsMap.get(sessionId);
        if (sessionStructures == null) {
            sessionStructures = new ArrayList<>();
            mSessionsMap.put(sessionId, sessionStructures);
        }
        sessionStructures.addAll(streamStructures);
        tracker.stop("", "commitSessionMutation", "mutations", streamStructures.size());
        return Boolean.TRUE;
    }

    @Override
    public SemanticPropertiesMutation editSemanticProperties() {
        return new FeedSemanticPropertiesMutation(this::commitSemanticPropertiesMutation);
    }

    private CommitResult commitSemanticPropertiesMutation(
            Map<String, ByteString> semanticPropertiesMap) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        this.mSemanticPropertiesMap.putAll(semanticPropertiesMap);
        tracker.stop(
                "", "commitSemanticPropertiesMutation", "mutations", semanticPropertiesMap.size());
        return CommitResult.SUCCESS;
    }

    @Override
    public UploadableActionMutation editUploadableActions() {
        return new FeedUploadableActionMutation(this::commitUploadableActionMutation);
    }

    private CommitResult commitUploadableActionMutation(
            Map<String, FeedUploadableActionMutation.FeedUploadableActionChanges> actions) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        CommitResult commitResult = CommitResult.SUCCESS;
        for (Map.Entry<String, FeedUploadableActionMutation.FeedUploadableActionChanges> entry :
                actions.entrySet()) {
            String contentId = entry.getKey();
            FeedUploadableActionMutation.FeedUploadableActionChanges changes = entry.getValue();
            Set<StreamUploadableAction> actionsSet = mUploadableActionsMap.get(contentId);
            if (actionsSet == null) {
                actionsSet = new HashSet<>();
            }
            actionsSet.removeAll(changes.removeActions());
            actionsSet.addAll(changes.upsertActions());
            mUploadableActionsMap.put(contentId, actionsSet);
        }
        tracker.stop("task", "commitUploadableActionMutation", "actions", actions.size());
        return commitResult;
    }

    @Override
    public LocalActionMutation editLocalActions() {
        return new FeedLocalActionMutation(this::commitLocalActionMutation);
    }

    private CommitResult commitLocalActionMutation(Map<Integer, List<String>> actions) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        CommitResult commitResult = CommitResult.SUCCESS;
        for (Map.Entry<Integer, List<String>> entry : actions.entrySet()) {
            Integer actionType = entry.getKey();
            List<StreamLocalAction> actionsList = mActionsMap.get(actionType);
            if (actionsList == null) {
                actionsList = new ArrayList<>();
                mActionsMap.put(actionType, actionsList);
            }
            for (String contentId : entry.getValue()) {
                StreamLocalAction action =
                        StreamLocalAction.newBuilder()
                                .setAction(actionType)
                                .setFeatureContentId(contentId)
                                .setTimestampSeconds(
                                        TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis()))
                                .build();
                actionsList.add(action);
            }
        }

        tracker.stop("task", "commitLocalActionMutation", "actions", actions.size());
        return commitResult;
    }

    @Override
    public Runnable triggerContentGc(Set<String> reservedContentIds,
            Supplier<Set<String>> accessibleContent, boolean keepSharedStates) {
        // No garbage collection in ephemeral mode
        return EMPTY_RUNNABLE;
    }

    @Override
    public Runnable triggerLocalActionGc(
            List<StreamLocalAction> actions, List<String> validContentIds) {
        // No garbage collection in ephemeral mode
        return EMPTY_RUNNABLE;
    }

    @Override
    public void switchToEphemeralMode() {
        // Do nothing
    }

    @Override
    public boolean isEphemeralMode() {
        return true;
    }

    @Override
    public void registerObserver(StoreListener observer) {
        throw new UnsupportedOperationException(
                "PersistentFeedStore does not support observer directly");
    }

    @Override
    public void unregisterObserver(StoreListener observer) {
        throw new UnsupportedOperationException(
                "PersistentFeedStore does not support observer directly");
    }

    @Override
    public boolean clearAll() {
        mPayloadWithIdMap.clear();
        mActionsMap.clear();
        mSemanticPropertiesMap.clear();
        mSessionsMap.clear();
        mSharedStateMap.clear();
        return true;
    }
}
