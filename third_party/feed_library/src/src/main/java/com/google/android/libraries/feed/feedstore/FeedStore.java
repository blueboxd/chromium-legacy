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

package com.google.android.libraries.feed.feedstore;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.lifecycle.Resettable;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.api.internal.store.SemanticPropertiesMutation;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.api.internal.store.StoreListener;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener;
import com.google.android.libraries.feed.feedstore.internal.ClearableStore;
import com.google.android.libraries.feed.feedstore.internal.EphemeralFeedStore;
import com.google.android.libraries.feed.feedstore.internal.FeedStoreHelper;
import com.google.android.libraries.feed.feedstore.internal.PersistentFeedStore;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * {@link Store} implementation that delegates between {@link PersistentFeedStore} and {@link
 * EphemeralFeedStore}.
 *
 * <p>TODO: We need to design the StoreListener support correctly. For the switch to
 * ephemeral mode, the Observers are called from here. If we ever have events which are raised by
 * one of the delegates, we need to make sure the registered observers are called correctly,
 * independent of what delegate is actually running. The delegates currently throw a
 * IllegalStateException if the register/unregister methods are called.
 */
public class FeedStore extends FeedObservable<StoreListener>
    implements Store, Resettable, FeedLifecycleListener {
  private static final String TAG = "FeedStore";

  // Permanent reference to the persistent store (used for setting off cleanup)
  private final PersistentFeedStore persistentStore;
  private final MainThreadRunner mainThreadRunner;
  // The current store
  private ClearableStore delegate;

  private final TaskQueue taskQueue;

  // Needed for switching to ephemeral mode
  private final FeedStoreHelper storeHelper = new FeedStoreHelper();
  private final BasicLoggingApi basicLoggingApi;
  private final Clock clock;
  private final TimingUtils timingUtils;
  private final ThreadUtils threadUtils;

  private boolean isEphemeralMode = false;

  protected final ContentStorageDirect contentStorage;
  protected final JournalStorageDirect journalStorage;

  public FeedStore(
      Configuration configuration,
      TimingUtils timingUtils,
      FeedExtensionRegistry extensionRegistry,
      ContentStorageDirect contentStorage,
      JournalStorageDirect journalStorage,
      ThreadUtils threadUtils,
      TaskQueue taskQueue,
      Clock clock,
      BasicLoggingApi basicLoggingApi,
      MainThreadRunner mainThreadRunner) {
    this.taskQueue = taskQueue;
    this.clock = clock;
    this.timingUtils = timingUtils;
    this.threadUtils = threadUtils;
    this.basicLoggingApi = basicLoggingApi;
    this.mainThreadRunner = mainThreadRunner;
    this.contentStorage = contentStorage;
    this.journalStorage = journalStorage;

    this.persistentStore =
        new PersistentFeedStore(
            configuration,
            this.timingUtils,
            extensionRegistry,
            contentStorage,
            journalStorage,
            this.taskQueue,
            threadUtils,
            this.clock,
            this.storeHelper,
            basicLoggingApi,
            mainThreadRunner);
    delegate = persistentStore;
  }

  @Override
  public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
    return delegate.getPayloads(contentIds);
  }

  @Override
  public Result<List<StreamSharedState>> getSharedStates() {
    return delegate.getSharedStates();
  }

  @Override
  public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
    return delegate.getStreamStructures(sessionId);
  }

  @Override
  public Result<List<String>> getAllSessions() {
    return delegate.getAllSessions();
  }

  @Override
  public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
    return delegate.getSemanticProperties(contentIds);
  }

  @Override
  public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
    return delegate.getAllDismissLocalActions();
  }

  @Override
  public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
    return delegate.getAllUploadableActions();
  }

  @Override
  public Result<String> createNewSession() {
    return delegate.createNewSession();
  }

  @Override
  public void removeSession(String sessionId) {
    delegate.removeSession(sessionId);
  }

  @Override
  public void clearHead() {
    delegate.clearHead();
  }

  @Override
  public ContentMutation editContent() {
    return delegate.editContent();
  }

  @Override
  public SessionMutation editSession(String sessionId) {
    return delegate.editSession(sessionId);
  }

  @Override
  public SemanticPropertiesMutation editSemanticProperties() {
    return delegate.editSemanticProperties();
  }

  @Override
  public LocalActionMutation editLocalActions() {
    return delegate.editLocalActions();
  }

  @Override
  public UploadableActionMutation editUploadableActions() {
    return delegate.editUploadableActions();
  }

  @Override
  public Runnable triggerContentGc(
      Set<String> reservedContentIds,
      Supplier<Set<String>> accessibleContent,
      boolean keepSharedStates) {
    return delegate.triggerContentGc(reservedContentIds, accessibleContent, keepSharedStates);
  }

  @Override
  public Runnable triggerLocalActionGc(
      List<StreamLocalAction> actions, List<String> validContentIds) {
    return delegate.triggerLocalActionGc(actions, validContentIds);
  }

  @Override
  public boolean isEphemeralMode() {
    return isEphemeralMode;
  }

  @Override
  public void switchToEphemeralMode() {
    // This should be called on a background thread because it's called during error handling.
    threadUtils.checkNotMainThread();
    if (!isEphemeralMode) {
      persistentStore.switchToEphemeralMode();
      delegate = new EphemeralFeedStore(clock, timingUtils, storeHelper);

      taskQueue.execute(
          Task.CLEAR_PERSISTENT_STORE_TASK,
          TaskType.BACKGROUND,
          () -> {
            ElapsedTimeTracker tracker = timingUtils.getElapsedTimeTracker(TAG);
            // Try to just wipe content + sessions
            boolean clearSuccess = persistentStore.clearNonActionContent();
            // If that fails, wipe everything.
            if (!clearSuccess) {
              persistentStore.clearAll();
            }
            tracker.stop("clearPersistentStore", "completed");
          });

      isEphemeralMode = true;
      synchronized (observers) {
        for (StoreListener listener : observers) {
          listener.onSwitchToEphemeralMode();
        }
      }
      mainThreadRunner.execute(
          "log ephemeral switch",
          () -> basicLoggingApi.onInternalError(InternalFeedError.SWITCH_TO_EPHEMERAL));
    }
  }

  @Override
  public void reset() {
    persistentStore.clearNonActionContent();
  }

  @Override
  public void onLifecycleEvent(@LifecycleEvent String event) {
    if (LifecycleEvent.ENTER_BACKGROUND.equals(event) && isEphemeralMode) {
      taskQueue.execute(
          Task.DUMP_EPHEMERAL_ACTIONS, TaskType.BACKGROUND, this::dumpEphemeralActions);
    }
  }

  private void dumpEphemeralActions() {
    // Get all action-related content (actions + semantic data)
    Result<List<StreamLocalAction>> dismissActionsResult = delegate.getAllDismissLocalActions();
    if (!dismissActionsResult.isSuccessful()) {
      Logger.e(TAG, "Error retrieving actions when trying to dump ephemeral actions.");
      return;
    }
    List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();
    LocalActionMutation localActionMutation = persistentStore.editLocalActions();
    List<String> dismissActionContentIds = new ArrayList<>(dismissActions.size());
    for (StreamLocalAction dismiss : dismissActions) {
      dismissActionContentIds.add(dismiss.getFeatureContentId());
      localActionMutation.add(dismiss.getAction(), dismiss.getFeatureContentId());
    }
    Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
        delegate.getSemanticProperties(dismissActionContentIds);
    if (!semanticPropertiesResult.isSuccessful()) {
      Logger.e(TAG, "Error retrieving semantic properties when trying to dump ephemeral actions.");
      return;
    }
    SemanticPropertiesMutation semanticPropertiesMutation =
        persistentStore.editSemanticProperties();
    for (SemanticPropertiesWithId semanticProperties : semanticPropertiesResult.getValue()) {
      semanticPropertiesMutation.add(
          semanticProperties.contentId, ByteString.copyFrom(semanticProperties.semanticData));
    }

    // Attempt to write action-related content to persistent storage
    CommitResult commitResult = localActionMutation.commit();
    if (commitResult != CommitResult.SUCCESS) {
      Logger.e(TAG, "Error writing actions to persistent store when dumping ephemeral actions.");
      return;
    }
    commitResult = semanticPropertiesMutation.commit();
    if (commitResult != CommitResult.SUCCESS) {
      Logger.e(
          TAG,
          "Error writing semantic properties to persistent store when dumping ephemeral actions.");
    }
  }
}
