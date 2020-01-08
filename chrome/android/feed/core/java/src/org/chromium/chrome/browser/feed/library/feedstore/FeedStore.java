// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore;

import com.google.protobuf.ByteString;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.lifecycle.Resettable;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener;
import org.chromium.chrome.browser.feed.library.feedstore.internal.ClearableStore;
import org.chromium.chrome.browser.feed.library.feedstore.internal.EphemeralFeedStore;
import org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreHelper;
import org.chromium.chrome.browser.feed.library.feedstore.internal.PersistentFeedStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

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
public class FeedStore
        extends FeedObservable<StoreListener> implements Store, Resettable, FeedLifecycleListener {
    private static final String TAG = "FeedStore";

    // Permanent reference to the persistent store (used for setting off cleanup)
    private final PersistentFeedStore mPersistentStore;
    private final MainThreadRunner mMainThreadRunner;
    // The current store
    private ClearableStore mDelegate;

    private final TaskQueue mTaskQueue;

    // Needed for switching to ephemeral mode
    private final FeedStoreHelper mStoreHelper = new FeedStoreHelper();
    private final BasicLoggingApi mBasicLoggingApi;
    private final Clock mClock;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;

    private boolean mIsEphemeralMode;

    protected final ContentStorageDirect mContentStorage;
    protected final JournalStorageDirect mJournalStorage;

    public FeedStore(Configuration configuration, TimingUtils timingUtils,
            FeedExtensionRegistry extensionRegistry, ContentStorageDirect contentStorage,
            JournalStorageDirect journalStorage, ThreadUtils threadUtils, TaskQueue taskQueue,
            Clock clock, BasicLoggingApi basicLoggingApi, MainThreadRunner mainThreadRunner) {
        this.mTaskQueue = taskQueue;
        this.mClock = clock;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtils;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mMainThreadRunner = mainThreadRunner;
        this.mContentStorage = contentStorage;
        this.mJournalStorage = journalStorage;

        this.mPersistentStore = new PersistentFeedStore(configuration, this.mTimingUtils,
                extensionRegistry, contentStorage, journalStorage, this.mTaskQueue, threadUtils,
                this.mClock, this.mStoreHelper, basicLoggingApi, mainThreadRunner);
        mDelegate = mPersistentStore;
    }

    @Override
    public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
        return mDelegate.getPayloads(contentIds);
    }

    @Override
    public Result<List<StreamSharedState>> getSharedStates() {
        return mDelegate.getSharedStates();
    }

    @Override
    public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
        return mDelegate.getStreamStructures(sessionId);
    }

    @Override
    public Result<List<String>> getAllSessions() {
        return mDelegate.getAllSessions();
    }

    @Override
    public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
        return mDelegate.getSemanticProperties(contentIds);
    }

    @Override
    public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
        return mDelegate.getAllDismissLocalActions();
    }

    @Override
    public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
        return mDelegate.getAllUploadableActions();
    }

    @Override
    public Result<String> createNewSession() {
        return mDelegate.createNewSession();
    }

    @Override
    public void removeSession(String sessionId) {
        mDelegate.removeSession(sessionId);
    }

    @Override
    public void clearHead() {
        mDelegate.clearHead();
    }

    @Override
    public ContentMutation editContent() {
        return mDelegate.editContent();
    }

    @Override
    public SessionMutation editSession(String sessionId) {
        return mDelegate.editSession(sessionId);
    }

    @Override
    public SemanticPropertiesMutation editSemanticProperties() {
        return mDelegate.editSemanticProperties();
    }

    @Override
    public LocalActionMutation editLocalActions() {
        return mDelegate.editLocalActions();
    }

    @Override
    public UploadableActionMutation editUploadableActions() {
        return mDelegate.editUploadableActions();
    }

    @Override
    public Runnable triggerContentGc(Set<String> reservedContentIds,
            Supplier<Set<String>> accessibleContent, boolean keepSharedStates) {
        return mDelegate.triggerContentGc(reservedContentIds, accessibleContent, keepSharedStates);
    }

    @Override
    public Runnable triggerLocalActionGc(
            List<StreamLocalAction> actions, List<String> validContentIds) {
        return mDelegate.triggerLocalActionGc(actions, validContentIds);
    }

    @Override
    public boolean isEphemeralMode() {
        return mIsEphemeralMode;
    }

    @Override
    public void switchToEphemeralMode() {
        // This should be called on a background thread because it's called during error handling.
        mThreadUtils.checkNotMainThread();
        if (!mIsEphemeralMode) {
            mPersistentStore.switchToEphemeralMode();
            mDelegate = new EphemeralFeedStore(mClock, mTimingUtils, mStoreHelper);

            mTaskQueue.execute(Task.CLEAR_PERSISTENT_STORE_TASK, TaskType.BACKGROUND, () -> {
                ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
                // Try to just wipe content + sessions
                boolean clearSuccess = mPersistentStore.clearNonActionContent();
                // If that fails, wipe everything.
                if (!clearSuccess) {
                    mPersistentStore.clearAll();
                }
                tracker.stop("clearPersistentStore", "completed");
            });

            mIsEphemeralMode = true;
            synchronized (mObservers) {
                for (StoreListener listener : mObservers) {
                    listener.onSwitchToEphemeralMode();
                }
            }
            mMainThreadRunner.execute("log ephemeral switch",
                    () -> mBasicLoggingApi.onInternalError(InternalFeedError.SWITCH_TO_EPHEMERAL));
        }
    }

    @Override
    public void reset() {
        mPersistentStore.clearNonActionContent();
    }

    @Override
    public void onLifecycleEvent(@LifecycleEvent String event) {
        if (LifecycleEvent.ENTER_BACKGROUND.equals(event) && mIsEphemeralMode) {
            mTaskQueue.execute(
                    Task.DUMP_EPHEMERAL_ACTIONS, TaskType.BACKGROUND, this::dumpEphemeralActions);
        }
    }

    private void dumpEphemeralActions() {
        // Get all action-related content (actions + semantic data)
        Result<List<StreamLocalAction>> dismissActionsResult =
                mDelegate.getAllDismissLocalActions();
        if (!dismissActionsResult.isSuccessful()) {
            Logger.e(TAG, "Error retrieving actions when trying to dump ephemeral actions.");
            return;
        }
        List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();
        LocalActionMutation localActionMutation = mPersistentStore.editLocalActions();
        List<String> dismissActionContentIds = new ArrayList<>(dismissActions.size());
        for (StreamLocalAction dismiss : dismissActions) {
            dismissActionContentIds.add(dismiss.getFeatureContentId());
            localActionMutation.add(dismiss.getAction(), dismiss.getFeatureContentId());
        }
        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                mDelegate.getSemanticProperties(dismissActionContentIds);
        if (!semanticPropertiesResult.isSuccessful()) {
            Logger.e(TAG,
                    "Error retrieving semantic properties when trying to dump ephemeral actions.");
            return;
        }
        SemanticPropertiesMutation semanticPropertiesMutation =
                mPersistentStore.editSemanticProperties();
        for (SemanticPropertiesWithId semanticProperties : semanticPropertiesResult.getValue()) {
            semanticPropertiesMutation.add(semanticProperties.contentId,
                    ByteString.copyFrom(semanticProperties.semanticData));
        }

        // Attempt to write action-related content to persistent storage
        CommitResult commitResult = localActionMutation.commit();
        if (commitResult != CommitResult.SUCCESS) {
            Logger.e(TAG,
                    "Error writing actions to persistent store when dumping ephemeral actions.");
            return;
        }
        commitResult = semanticPropertiesMutation.commit();
        if (commitResult != CommitResult.SUCCESS) {
            Logger.e(TAG,
                    "Error writing semantic properties to persistent store when dumping"
                            + " ephemeral actions.");
        }
    }
}
