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

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelMutation;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Validators;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import java.util.List;
import java.util.Set;

/** Implementation of a {@link Session}. */
// TODO: Tiktok doesn't allow HeadSessionImpl to extend SessionImpl
public class SessionImpl implements InitializableSession, Dumpable {
  private static final String TAG = "SessionImpl";

  protected final Store store;
  protected final TaskQueue taskQueue;
  protected final ThreadUtils threadUtils;
  protected final TimingUtils timingUtils;
  private final boolean limitPagingUpdates;
  private final SessionContentTracker sessionContentTracker =
      new SessionContentTracker(/* supportsClearAll= */ false);

  // Allow creation of the session without a model provider, this becomes an unbound session
  /*@Nullable*/ protected ModelProvider modelProvider;
  /*@Nullable*/ protected ViewDepthProvider viewDepthProvider;
  protected boolean legacyHeadContent = false;

  protected String sessionId;

  // operation counts for the dumper
  int updateCount = 0;

  SessionImpl(
      Store store,
      boolean limitPagingUpdates,
      TaskQueue taskQueue,
      TimingUtils timingUtils,
      ThreadUtils threadUtils) {
    this.store = store;
    this.taskQueue = taskQueue;
    this.timingUtils = timingUtils;
    this.threadUtils = threadUtils;
    this.limitPagingUpdates = limitPagingUpdates;
  }

  @Override
  public void bindModelProvider(
      /*@Nullable*/ ModelProvider modelProvider, /*@Nullable*/ ViewDepthProvider viewDepthProvider) {
    this.modelProvider = modelProvider;
    this.viewDepthProvider = viewDepthProvider;
  }

  @Override
  public void setSessionId(String sessionId) {
    this.sessionId = sessionId;
  }

  @Override
  /*@Nullable*/
  public ModelProvider getModelProvider() {
    return modelProvider;
  }

  @Override
  public void populateModelProvider(
      List<StreamStructure> head,
      boolean cachedBindings,
      boolean legacyHeadContent,
      UiContext uiContext) {
    Logger.i(TAG, "PopulateModelProvider %s items", head.size());
    this.legacyHeadContent = legacyHeadContent;
    threadUtils.checkNotMainThread();
    ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);

    if (modelProvider != null) {
      ModelMutation modelMutation =
          modelProvider
              .edit()
              .hasCachedBindings(cachedBindings)
              .setSessionId(sessionId)
              .setMutationContext(new MutationContext.Builder().setUiContext(uiContext).build());

      // Walk through head and add all the DataOperations to the session.
      for (StreamStructure streamStructure : head) {
        String contentId = streamStructure.getContentId();
        switch (streamStructure.getOperation()) {
          case UPDATE_OR_APPEND:
            if (!sessionContentTracker.contains(contentId)) {
              modelMutation.addChild(streamStructure);
            }
            break;
          case REMOVE:
            modelMutation.removeChild(streamStructure);
            break;
          case REQUIRED_CONTENT:
            // No-op, required content should not be passed into modelMutation.
            break;
          default:
            Logger.e(TAG, "unsupported StreamDataOperation: %s", streamStructure.getOperation());
        }
        sessionContentTracker.update(streamStructure);
      }
      modelMutation.commit();
    } else {
      sessionContentTracker.update(head);
    }

    timeTracker.stop("populateSession", sessionId, "operations", head.size());
  }

  @Override
  public void updateSession(
      boolean clearHead,
      List<StreamStructure> streamStructures,
      int schemaVersion,
      /*@Nullable*/ MutationContext mutationContext) {
    String localSessionId = Validators.checkNotNull(sessionId);
    if (clearHead) {
      if (shouldInvalidateModelProvider(mutationContext, localSessionId)) {
        if (modelProvider != null) {
          modelProvider.invalidate();
          Logger.i(
              TAG,
              "Invalidating Model Provider for session %s due to a clear head",
              localSessionId);
        }
      } else {
        Logger.i(TAG, "Session %s not updated due to clearHead", localSessionId);
      }
      return;
    }
    updateCount++;
    updateSessionInternal(streamStructures, mutationContext);
  }

  protected boolean shouldInvalidateModelProvider(
      /*@Nullable*/ MutationContext mutationContext, String sessionId) {
    if (modelProvider != null
        && mutationContext != null
        && mutationContext.getContinuationToken() != null) {
      return sessionId.equals(mutationContext.getRequestingSessionId());
    }
    return false;
  }

  @Override
  public boolean invalidateOnResetHead() {
    return true;
  }

  void updateSessionInternal(
      List<StreamStructure> streamStructures, /*@Nullable*/ MutationContext mutationContext) {

    ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
    StreamToken mutationSourceToken =
        mutationContext != null ? mutationContext.getContinuationToken() : null;
    if (mutationSourceToken != null) {
      String contentId = mutationSourceToken.getContentId();
      if (!sessionContentTracker.contains(contentId)) {
        // Make sure that mutationSourceToken is a token in this session, if not, we don't update
        // the session.
        timeTracker.stop("updateSessionIgnored", sessionId, "Token Not Found", contentId);
        Logger.i(TAG, "Token %s not found in session, ignoring update", contentId);
        return;
      } else if (limitPagingUpdates) {
        String mutationSessionId =
            Validators.checkNotNull(mutationContext).getRequestingSessionId();
        if (mutationSessionId == null) {
          Logger.i(TAG, "Request Session was not set, ignoring update");
          return;
        }
        if (!sessionId.equals(mutationSessionId)) {
          Logger.i(TAG, "Limiting Updates, paging request made on another session");
          return;
        }
      }
    }

    ModelMutation modelMutation = (modelProvider != null) ? modelProvider.edit() : null;
    if (modelMutation != null && mutationContext != null) {
      modelMutation.setMutationContext(mutationContext);
      if (mutationContext.getContinuationToken() != null) {
        modelMutation.hasCachedBindings(true);
      }
    }

    int updateCnt = 0;
    int addFeatureCnt = 0;
    int requiredContentCnt = 0;
    SessionMutation sessionMutation = store.editSession(sessionId);
    for (StreamStructure streamStructure : streamStructures) {
      String contentId = streamStructure.getContentId();
      switch (streamStructure.getOperation()) {
        case UPDATE_OR_APPEND:
          if (sessionContentTracker.contains(contentId)) {
            // TODO: This could leave an empty feature if contentKey already exists in
            // the session with a different parent.
            if (modelMutation != null) {
              // this is an update operation so we can ignore it
              modelMutation.updateChild(streamStructure);
              updateCnt++;
            }
          } else {
            sessionMutation.add(streamStructure);
            if (modelMutation != null) {
              modelMutation.addChild(streamStructure);
            }
            addFeatureCnt++;
          }
          break;
        case REMOVE:
          sessionMutation.add(streamStructure);
          if (modelMutation != null) {
            modelMutation.removeChild(streamStructure);
          }
          break;
        case CLEAR_ALL:
          Logger.i(TAG, "CLEAR_ALL not support on this session type");
          break;
        case REQUIRED_CONTENT:
          if (!sessionContentTracker.contains(contentId)) {
            sessionMutation.add(streamStructure);
            requiredContentCnt++;
          }
          break;
        default:
          Logger.e(TAG, "Unknown operation, ignoring: %s", streamStructure.getOperation());
      }
      sessionContentTracker.update(streamStructure);
    }

    // Commit the Model Provider mutation after the store is updated.
    int taskType =
        mutationContext != null && mutationContext.isUserInitiated()
            ? TaskType.IMMEDIATE
            : TaskType.USER_FACING;
    taskQueue.execute(Task.SESSION_MUTATION, taskType, sessionMutation::commit);
    if (modelMutation != null) {
      modelMutation.commit();
    }
    timeTracker.stop(
        "updateSession",
        sessionId,
        "features",
        addFeatureCnt,
        "updates",
        updateCnt,
        "requiredContent",
        requiredContentCnt);
  }

  @Override
  public String getSessionId() {
    return Validators.checkNotNull(sessionId);
  }

  @Override
  public Set<String> getContentInSession() {
    return sessionContentTracker.getContentIds();
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    dumper.forKey("sessionName").value(sessionId);
    dumper
        .forKey("")
        .value((modelProvider == null) ? "sessionIsUnbound" : "sessionIsBound")
        .compactPrevious();
    dumper.forKey("updateCount").value(updateCount).compactPrevious();
    dumper.dump(sessionContentTracker);
    if (modelProvider instanceof Dumpable) {
      dumper.dump((Dumpable) modelProvider);
    }
  }
}
