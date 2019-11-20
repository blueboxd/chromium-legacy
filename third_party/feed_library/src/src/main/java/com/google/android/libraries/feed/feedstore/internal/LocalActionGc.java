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

  private final List<StreamLocalAction> actions;
  private final List<String> validContentIds;
  private final JournalStorageDirect journalStorageDirect;
  private final TimingUtils timingUtils;
  private final String journalName;

  LocalActionGc(
      List<StreamLocalAction> actions,
      List<String> validContentIds,
      JournalStorageDirect journalStorageDirect,
      TimingUtils timingUtils,
      String journalName) {

    this.actions = actions;
    this.validContentIds = validContentIds;
    this.journalStorageDirect = journalStorageDirect;
    this.timingUtils = timingUtils;
    this.journalName = journalName;
  }

  /**
   * Cleans up the store based on {@link #actions} and {@link #validContentIds}. Any valid actions
   * will be copied over to a new copy of the action journal.
   */
  void gc() {
    ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
    List<StreamLocalAction> validActions = new ArrayList<>(validContentIds.size());

    for (StreamLocalAction action : actions) {
      if (validContentIds.contains(action.getFeatureContentId())) {
        validActions.add(action);
      }
    }

    Builder mutationBuilder = new Builder(journalName);
    mutationBuilder.delete();

    for (StreamLocalAction action : validActions) {
      mutationBuilder.append(action.toByteArray());
    }
    CommitResult result = journalStorageDirect.commit(mutationBuilder.build());
    if (result == CommitResult.SUCCESS) {
      tracker.stop("gcMutation", actions.size() - validActions.size());
    } else {
      tracker.stop("gcMutation failed");
    }
  }
}
