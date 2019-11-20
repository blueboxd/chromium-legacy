// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.SemanticPropertiesMutation;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.StoreListener;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
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

  private final Clock clock;
  private final TimingUtils timingUtils;
  private final FeedStoreHelper storeHelper;

  private final Map<String, PayloadWithId> payloadWithIdMap = new HashMap<>();
  private final Map<String, StreamSharedState> sharedStateMap = new HashMap<>();
  private final Map<String, ByteString> semanticPropertiesMap = new HashMap<>();
  private final Map<Integer, List<StreamLocalAction>> actionsMap = new HashMap<>();
  private final Map<String, Set<StreamUploadableAction>> uploadableActionsMap = new HashMap<>();
  private final Map<String, List<StreamStructure>> sessionsMap = new HashMap<>();

  public EphemeralFeedStore(Clock clock, TimingUtils timingUtils, FeedStoreHelper storeHelper) {
    this.clock = clock;
    this.timingUtils = timingUtils;
    this.storeHelper = storeHelper;
  }

  @Override
  public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
    List<PayloadWithId> payloads = new ArrayList<>(contentIds.size());
    for (String contentId : contentIds) {
      PayloadWithId payload = payloadWithIdMap.get(contentId);
      if (payload != null) {
        payloads.add(payload);
      }
    }
    return Result.success(payloads);
  }

  @Override
  public Result<List<StreamSharedState>> getSharedStates() {
    return Result.success(Collections.unmodifiableList(new ArrayList<>(sharedStateMap.values())));
  }

  @Override
  public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
    List<StreamStructure> streamStructures = sessionsMap.get(sessionId);
    if (streamStructures == null) {
      streamStructures = Collections.emptyList();
    }
    return Result.success(streamStructures);
  }

  @Override
  public Result<List<String>> getAllSessions() {
    Set<String> sessions = sessionsMap.keySet();
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
    List<SemanticPropertiesWithId> semanticPropertiesWithIds = new ArrayList<>(contentIds.size());
    for (String contentId : contentIds) {
      ByteString semanticProperties = semanticPropertiesMap.get(contentId);
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
    List<StreamLocalAction> dismissActions = actionsMap.get(ActionType.DISMISS);
    if (dismissActions == null) {
      dismissActions = Collections.emptyList();
    }
    return Result.success(dismissActions);
  }

  @Override
  public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
    Set<StreamUploadableAction> uploadableActions = Collections.emptySet();
    for (Set<StreamUploadableAction> actions : uploadableActionsMap.values()) {
      uploadableActions.addAll(actions);
    }
    return Result.success(uploadableActions);
  }

  @Override
  public Result<String> createNewSession() {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    String sessionId = storeHelper.getNewStreamSessionId();
    Result<List<StreamStructure>> streamStructuresResult = getStreamStructures(HEAD_SESSION_ID);
    sessionsMap.put(sessionId, new ArrayList<>(streamStructuresResult.getValue()));
    tracker.stop("createNewSession", sessionId);
    return Result.success(sessionId);
  }

  @Override
  public void removeSession(String sessionId) {
    if (sessionId.equals(HEAD_SESSION_ID)) {
      throw new IllegalStateException("Unable to delete the $HEAD session");
    }
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    sessionsMap.remove(sessionId);
    tracker.stop("removeSession", sessionId);
  }

  @Override
  public void clearHead() {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    sessionsMap.remove(HEAD_SESSION_ID);
    tracker.stop("", "clearHead");
  }

  @Override
  public ContentMutation editContent() {
    return new FeedContentMutation(this::commitContentMutation);
  }

  private CommitResult commitContentMutation(List<PayloadWithId> mutations) {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);

    for (PayloadWithId mutation : mutations) {
      String contentId = mutation.contentId;
      if (mutation.payload.hasStreamSharedState()) {
        StreamSharedState streamSharedState = mutation.payload.getStreamSharedState();
        sharedStateMap.put(contentId, streamSharedState);
      } else {
        payloadWithIdMap.put(contentId, mutation);
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

  private Boolean commitSessionMutation(String sessionId, List<StreamStructure> streamStructures) {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    List<StreamStructure> sessionStructures = sessionsMap.get(sessionId);
    if (sessionStructures == null) {
      sessionStructures = new ArrayList<>();
      sessionsMap.put(sessionId, sessionStructures);
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
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    this.semanticPropertiesMap.putAll(semanticPropertiesMap);
    tracker.stop("", "commitSemanticPropertiesMutation", "mutations", semanticPropertiesMap.size());
    return CommitResult.SUCCESS;
  }

  @Override
  public UploadableActionMutation editUploadableActions() {
    return new FeedUploadableActionMutation(this::commitUploadableActionMutation);
  }

  private CommitResult commitUploadableActionMutation(
      Map<String, FeedUploadableActionMutation.FeedUploadableActionChanges> actions) {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    CommitResult commitResult = CommitResult.SUCCESS;
    for (Map.Entry<String, FeedUploadableActionMutation.FeedUploadableActionChanges> entry :
        actions.entrySet()) {
      String contentId = entry.getKey();
      FeedUploadableActionMutation.FeedUploadableActionChanges changes = entry.getValue();
      Set<StreamUploadableAction> actionsSet = uploadableActionsMap.get(contentId);
      if (actionsSet == null) {
        actionsSet = new HashSet<>();
      }
      actionsSet.removeAll(changes.removeActions());
      actionsSet.addAll(changes.upsertActions());
      uploadableActionsMap.put(contentId, actionsSet);
    }
    tracker.stop("task", "commitUploadableActionMutation", "actions", actions.size());
    return commitResult;
  }

  @Override
  public LocalActionMutation editLocalActions() {
    return new FeedLocalActionMutation(this::commitLocalActionMutation);
  }

  private CommitResult commitLocalActionMutation(Map<Integer, List<String>> actions) {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    CommitResult commitResult = CommitResult.SUCCESS;
    for (Map.Entry<Integer, List<String>> entry : actions.entrySet()) {
      Integer actionType = entry.getKey();
      List<StreamLocalAction> actionsList = actionsMap.get(actionType);
      if (actionsList == null) {
        actionsList = new ArrayList<>();
        actionsMap.put(actionType, actionsList);
      }
      for (String contentId : entry.getValue()) {
        StreamLocalAction action =
            StreamLocalAction.newBuilder()
                .setAction(actionType)
                .setFeatureContentId(contentId)
                .setTimestampSeconds(TimeUnit.MILLISECONDS.toSeconds(clock.currentTimeMillis()))
                .build();
        actionsList.add(action);
      }
    }

    tracker.stop("task", "commitLocalActionMutation", "actions", actions.size());
    return commitResult;
  }

  @Override
  public Runnable triggerContentGc(
      Set<String> reservedContentIds,
      Supplier<Set<String>> accessibleContent,
      boolean keepSharedStates) {
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
    payloadWithIdMap.clear();
    actionsMap.clear();
    semanticPropertiesMap.clear();
    sessionsMap.clear();
    sharedStateMap.clear();
    return true;
  }
}
