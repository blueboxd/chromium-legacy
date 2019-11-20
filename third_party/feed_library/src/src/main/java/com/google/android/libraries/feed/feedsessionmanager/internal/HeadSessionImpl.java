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
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import java.util.List;
import java.util.Set;

/**
 * Implementation of {@link Session} for $HEAD. This class doesn't support a ModelProvider. The
 * $HEAD session does not support optimistic writes because we may create a new session between when
 * the response is received and when task updating the head session runs.
 */
public class HeadSessionImpl implements Session, Dumpable {
  private static final String TAG = "HeadSessionImpl";

  private final Store store;
  private final TimingUtils timingUtils;
  private final SessionContentTracker sessionContentTracker =
      new SessionContentTracker(/* supportsClearAll= */ true);
  private final boolean limitPageUpdatesInHead;

  private int schemaVersion = 0;

  // operation counts for the dumper
  private int updateCount = 0;
  private int storeMutationFailures = 0;

  HeadSessionImpl(Store store, TimingUtils timingUtils, boolean limitPageUpdatesInHead) {
    this.store = store;
    this.timingUtils = timingUtils;
    this.limitPageUpdatesInHead = limitPageUpdatesInHead;
  }

  /** Initialize head from the stored stream structures. */
  void initializeSession(List<StreamStructure> streamStructures, int schemaVersion) {
    Logger.i(TAG, "Initialize HEAD %s items", streamStructures.size());
    this.schemaVersion = schemaVersion;
    sessionContentTracker.update(streamStructures);
  }

  void reset() {
    sessionContentTracker.clear();
  }

  public int getSchemaVersion() {
    return schemaVersion;
  }

  @Override
  public boolean invalidateOnResetHead() {
    // There won't be a ModelProvider to invalidate
    return false;
  }

  @Override
  public void updateSession(
      boolean clearHead,
      List<StreamStructure> streamStructures,
      int schemaVersion,
      /*@Nullable*/ MutationContext mutationContext) {
    ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
    updateCount++;

    if (clearHead) {
      this.schemaVersion = schemaVersion;
    }

    StreamToken mutationSourceToken =
        mutationContext != null ? mutationContext.getContinuationToken() : null;
    if (mutationSourceToken != null) {
      String contentId = mutationSourceToken.getContentId();
      if (mutationSourceToken.hasContentId() && !sessionContentTracker.contains(contentId)) {
        // Make sure that mutationSourceToken is a token in this session, if not, we don't update
        // the session.
        timeTracker.stop("updateSessionIgnored", getSessionId(), "Token Not Found", contentId);
        Logger.i(TAG, "Token %s not found in session, ignoring update", contentId);
        return;
      } else if (limitPageUpdatesInHead) {
        timeTracker.stop("updateSessionIgnored", getSessionId());
        Logger.i(TAG, "Limited paging updates in HEAD");
        return;
      }
    }

    int updateCnt = 0;
    int addFeatureCnt = 0;
    int requiredContentCnt = 0;
    boolean cleared = false;
    SessionMutation sessionMutation = store.editSession(getSessionId());
    for (StreamStructure streamStructure : streamStructures) {
      String contentId = streamStructure.getContentId();
      switch (streamStructure.getOperation()) {
        case UPDATE_OR_APPEND:
          if (sessionContentTracker.contains(contentId)) {
            // this is an update operation so we can ignore it
            updateCnt++;
          } else {
            sessionMutation.add(streamStructure);
            addFeatureCnt++;
          }
          break;
        case REMOVE:
          Logger.i(TAG, "Removing Item %s from $HEAD", contentId);
          if (sessionContentTracker.contains(contentId)) {
            sessionMutation.add(streamStructure);
          } else {
            Logger.w(TAG, "Remove operation content not found in $HEAD");
          }
          break;
        case CLEAR_ALL:
          cleared = true;
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
    boolean success = sessionMutation.commit();
    if (success) {
      timeTracker.stop(
          "updateSession",
          getSessionId(),
          "cleared",
          cleared,
          "features",
          addFeatureCnt,
          "updates",
          updateCnt,
          "requiredContent",
          requiredContentCnt);
    } else {
      timeTracker.stop("updateSessionFailure", getSessionId());
      storeMutationFailures++;
      Logger.e(TAG, "$HEAD session mutation failed");
      store.switchToEphemeralMode();
    }
  }

  @Override
  public String getSessionId() {
    return Store.HEAD_SESSION_ID;
  }

  @Override
  /*@Nullable*/
  public ModelProvider getModelProvider() {
    return null;
  }

  @Override
  public Set<String> getContentInSession() {
    return sessionContentTracker.getContentIds();
  }

  public boolean isHeadEmpty() {
    return sessionContentTracker.isEmpty();
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    dumper.forKey("sessionName").value(getSessionId());
    dumper.forKey("updateCount").value(updateCount).compactPrevious();
    dumper.forKey("storeMutationFailures").value(storeMutationFailures).compactPrevious();
    dumper.dump(sessionContentTracker);
  }
}
