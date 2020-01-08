// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.DISMISS_ACTION_JOURNAL;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SEMANTIC_PROPERTIES_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.SHARED_STATE_PREFIX;
import static org.chromium.chrome.browser.feed.library.feedstore.internal.FeedStoreConstants.UPLOADABLE_ACTION_PREFIX;

import android.util.Base64;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.intern.Interner;
import org.chromium.chrome.browser.feed.library.common.intern.InternerWithStats;
import org.chromium.chrome.browser.feed.library.common.intern.WeakPoolInterner;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of the Store. The PersistentFeedStore will call the host APIs {@link
 * ContentStorage} and {@link JournalStorage} to make persistent changes.
 */
public final class PersistentFeedStore implements ClearableStore, Dumpable {
    private static final String TAG = "PersistentFeedStore";

    private final Configuration mConfiguration;
    private final TimingUtils mTimingUtils;
    private final FeedExtensionRegistry mExtensionRegistry;
    private final ContentStorageDirect mContentStorageDirect;
    private final JournalStorageDirect mJournalStorageDirect;
    private final TaskQueue mTaskQueue;
    private final ThreadUtils mThreadUtils;
    private final Clock mClock;
    private final FeedStoreHelper mStoreHelper;
    private final BasicLoggingApi mBasicLoggingApi;
    private final MainThreadRunner mMainThreadRunner;

    // We use a common string interner pool because the same content IDs are reused across different
    // protos. The actual proto interners below are backed by thisl common pool.
    private final InternerWithStats<String> mContentIdStringInterner =
            new InternerWithStats<>(new WeakPoolInterner<>());
    private final Interner<StreamStructure> mStreamStructureInterner =
            new StreamStructureInterner(mContentIdStringInterner);
    private final Interner<StreamPayload> mStreamPayloadInterner =
            new StreamPayloadInterner(mContentIdStringInterner);

    public PersistentFeedStore(Configuration configuration, TimingUtils timingUtils,
            FeedExtensionRegistry extensionRegistry, ContentStorageDirect contentStorageDirect,
            JournalStorageDirect journalStorageDirect, TaskQueue taskQueue, ThreadUtils threadUtils,
            Clock clock, FeedStoreHelper storeHelper, BasicLoggingApi basicLoggingApi,
            MainThreadRunner mainThreadRunner) {
        this.mConfiguration = configuration;
        this.mTimingUtils = timingUtils;
        this.mExtensionRegistry = extensionRegistry;
        this.mContentStorageDirect = contentStorageDirect;
        this.mJournalStorageDirect = journalStorageDirect;
        this.mTaskQueue = taskQueue;
        this.mThreadUtils = threadUtils;
        this.mClock = clock;
        this.mStoreHelper = storeHelper;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mMainThreadRunner = mainThreadRunner;
    }

    @Override
    public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<PayloadWithId> payloads = new ArrayList<>(contentIds.size());
        Result<Map<String, byte[]>> contentResult = mContentStorageDirect.get(contentIds);
        if (!contentResult.isSuccessful()) {
            Logger.e(TAG, "Unsuccessful fetching payloads for content ids %s", contentIds);
            tracker.stop("getPayloads failed", "items", contentIds);
            return Result.failure();
        }

        for (Map.Entry<String, byte[]> entry : contentResult.getValue().entrySet()) {
            try {
                StreamPayload streamPayload = mStreamPayloadInterner.intern(StreamPayload.parseFrom(
                        entry.getValue(), mExtensionRegistry.getExtensionRegistry()));
                payloads.add(new PayloadWithId(entry.getKey(), streamPayload));
            } catch (InvalidProtocolBufferException e) {
                Logger.e(TAG, "Couldn't parse content proto for id %s", entry.getKey());
                mMainThreadRunner.execute("ITEM_NOT_PARSED", () -> {
                    mBasicLoggingApi.onInternalError(InternalFeedError.ITEM_NOT_PARSED);
                });
            }
        }
        tracker.stop("", "getPayloads", "items", payloads.size());
        return Result.success(payloads);
    }

    @Override
    public Result<List<StreamSharedState>> getSharedStates() {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Result<Map<String, byte[]>> bytesResult = mContentStorageDirect.getAll(SHARED_STATE_PREFIX);
        if (!bytesResult.isSuccessful()) {
            Logger.e(TAG, "Error fetching shared states");
            tracker.stop("getSharedStates", "failed");
            return Result.failure();
        }
        Collection<byte[]> result = bytesResult.getValue().values();
        List<StreamSharedState> sharedStates = new ArrayList<>(result.size());
        for (byte[] byteArray : result) {
            try {
                sharedStates.add(StreamSharedState.parseFrom(byteArray));
            } catch (InvalidProtocolBufferException e) {
                Logger.e(TAG, e, "Error parsing protocol buffer from bytes %s", byteArray);
                tracker.stop("getSharedStates", "failed");
                return Result.failure();
            }
        }
        tracker.stop("", "getSharedStates", "items", sharedStates.size());
        return Result.success(sharedStates);
    }

    @Override
    public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<StreamStructure> streamStructures;
        Result<List<byte[]>> operationsResult = mJournalStorageDirect.read(sessionId);
        if (!operationsResult.isSuccessful()) {
            Logger.e(TAG, "Error fetching stream structures for session %s", sessionId);
            tracker.stop("getStreamStructures failed", "session", sessionId);
            return Result.failure();
        }
        List<byte[]> results = operationsResult.getValue();
        streamStructures = new ArrayList<>(results.size());
        for (byte[] bytes : results) {
            if (bytes.length == 0) {
                continue;
            }
            try {
                streamStructures.add(
                        mStreamStructureInterner.intern(StreamStructure.parseFrom(bytes)));
            } catch (InvalidProtocolBufferException e) {
                Logger.e(TAG, e, "Error parsing stream structure.");
            }
        }
        tracker.stop("", "getStreamStructures", "items", streamStructures.size());
        return Result.success(streamStructures);
    }

    @Override
    public Result<List<String>> getAllSessions() {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<String> sessions;

        Result<List<String>> namesResult = mJournalStorageDirect.getAllJournals();
        if (!namesResult.isSuccessful()) {
            Logger.e(TAG, "Error fetching all journals");
            tracker.stop("getAllSessions failed");
            return Result.failure();
        }

        List<String> result = namesResult.getValue();
        sessions = new ArrayList<>(result.size());
        for (String name : result) {
            if (HEAD_SESSION_ID.equals(name)) {
                // Don't add $HEAD to the sessions list
                continue;
            }
            if (DISMISS_ACTION_JOURNAL.equals(name)) {
                // Don't add Dismiss Actions journal to sessions list
                continue;
            }
            sessions.add(name);
        }
        tracker.stop("", "getAllSessions", "items", sessions.size());
        return Result.success(sessions);
    }

    @Override
    public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        List<SemanticPropertiesWithId> semanticPropertiesWithIds =
                new ArrayList<>(contentIds.size());
        List<String> contentIdKeys = new ArrayList<>(contentIds.size());
        for (String contentId : contentIds) {
            contentIdKeys.add(SEMANTIC_PROPERTIES_PREFIX + contentId);
        }
        Result<Map<String, byte[]>> mapResult = mContentStorageDirect.get(contentIdKeys);

        if (mapResult.isSuccessful()) {
            for (Map.Entry<String, byte[]> entry : mapResult.getValue().entrySet()) {
                String contentId = entry.getKey().replace(SEMANTIC_PROPERTIES_PREFIX, "");
                if (contentIds.contains(contentId)) {
                    semanticPropertiesWithIds.add(
                            new SemanticPropertiesWithId(contentId, entry.getValue()));
                }
            }
        } else {
            Logger.e(TAG, "Error fetching semantic properties for content ids %s", contentIds);
            tracker.stop("getSemanticProperties failed", contentIds);
        }

        tracker.stop("task", "getSemanticProperties", "size", semanticPropertiesWithIds.size());
        return Result.success(semanticPropertiesWithIds);
    }

    @Override
    public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);

        Result<List<byte[]>> listResult = mJournalStorageDirect.read(DISMISS_ACTION_JOURNAL);
        if (!listResult.isSuccessful()) {
            Logger.e(TAG, "Error retrieving dismiss journal");
            tracker.stop("getAllDismissLocalActions failed");
            return Result.failure();
        } else {
            List<byte[]> actionByteArrays = listResult.getValue();
            List<StreamLocalAction> actions = new ArrayList<>(actionByteArrays.size());
            for (byte[] bytes : actionByteArrays) {
                StreamLocalAction action;
                try {
                    action = StreamLocalAction.parseFrom(bytes);
                    actions.add(action);
                } catch (InvalidProtocolBufferException e) {
                    Logger.e(TAG, e, "Error parsing StreamLocalAction for bytes: %s (length %d)",
                            Base64.encodeToString(bytes, Base64.DEFAULT), bytes.length);
                }
            }
            tracker.stop("task", "getAllDismissLocalActions", "size", actions.size());
            return Result.success(actions);
        }
    }

    @Override
    public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Result<Map<String, byte[]>> bytesResult =
                mContentStorageDirect.getAll(UPLOADABLE_ACTION_PREFIX);
        if (!bytesResult.isSuccessful()) {
            Logger.e(TAG, "Error fetching shared states");
            tracker.stop("getAllUploadableActions", "failed");
            return Result.failure();
        }
        Collection<byte[]> result = bytesResult.getValue().values();
        Set<StreamUploadableAction> uploadableActions = new HashSet<>();
        for (byte[] byteArray : result) {
            try {
                uploadableActions.add(StreamUploadableAction.parseFrom(byteArray));
            } catch (InvalidProtocolBufferException e) {
                tracker.stop("getAllUploadableActions", "failed");
                Logger.e(TAG, e, "Error parsing protocol buffer from bytes %s", byteArray);
                return Result.failure();
            }
        }
        tracker.stop("", "getAllUploadableActions", "items", uploadableActions.size());
        return Result.success(uploadableActions);
    }

    @Override
    public Result<String> createNewSession() {
        mThreadUtils.checkNotMainThread();

        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        String sessionId = mStoreHelper.getNewStreamSessionId();
        mJournalStorageDirect.commit(
                new JournalMutation.Builder(HEAD_SESSION_ID).copy(sessionId).build());
        tracker.stop("createNewSession", sessionId);
        return Result.success(sessionId);
    }

    @Override
    public void removeSession(String sessionId) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        if (sessionId.equals(HEAD_SESSION_ID)) {
            throw new IllegalStateException("Unable to delete the $HEAD session");
        }
        mJournalStorageDirect.commit(new JournalMutation.Builder(sessionId).delete().build());
        tracker.stop("removeSession", sessionId);
    }

    @Override
    public void clearHead() {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        mJournalStorageDirect.commit(
                new JournalMutation.Builder(HEAD_SESSION_ID).delete().append(new byte[0]).build());
        tracker.stop("", "clearHead");
    }

    @Override
    public ContentMutation editContent() {
        mThreadUtils.checkNotMainThread();
        return new FeedContentMutation(this::commitContentMutation);
    }

    @Override
    public SessionMutation editSession(String sessionId) {
        mThreadUtils.checkNotMainThread();
        return new FeedSessionMutation(
                feedSessionMutation -> commitSessionMutation(sessionId, feedSessionMutation));
    }

    @Override
    public SemanticPropertiesMutation editSemanticProperties() {
        mThreadUtils.checkNotMainThread();
        return new FeedSemanticPropertiesMutation(this::commitSemanticPropertiesMutation);
    }

    @Override
    public LocalActionMutation editLocalActions() {
        mThreadUtils.checkNotMainThread();
        return new FeedLocalActionMutation(this::commitLocalActionMutation);
    }

    @Override
    public UploadableActionMutation editUploadableActions() {
        return new FeedUploadableActionMutation(this::commitUploadableActionMutation);
    }

    @Override
    public Runnable triggerContentGc(Set<String> reservedContentIds,
            Supplier<Set<String>> accessibleContent, boolean keepSharedStates) {
        Supplier<Set<StreamLocalAction>> dismissActionSupplier = () -> {
            Result<List<StreamLocalAction>> dismissActionsResult = getAllDismissLocalActions();

            if (!dismissActionsResult.isSuccessful()) {
                // TODO: clean up error condition
                Logger.e(TAG, "Error retrieving dismiss actions for content garbage collection");
                return Collections.emptySet();
            } else {
                return new HashSet<>(dismissActionsResult.getValue());
            }
        };

        return new ContentGc(mConfiguration, accessibleContent, reservedContentIds,
                dismissActionSupplier, mContentStorageDirect, mTaskQueue, mTimingUtils,
                keepSharedStates)::gc;
    }

    @Override
    public Runnable triggerLocalActionGc(
            List<StreamLocalAction> actions, List<String> validContentIds) {
        return new LocalActionGc(actions, validContentIds, mJournalStorageDirect, mTimingUtils,
                DISMISS_ACTION_JOURNAL)::gc;
    }

    @Override
    public void switchToEphemeralMode() {
        // TODO: implement cleanup
    }

    @Override
    public boolean isEphemeralMode() {
        return false;
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

    private CommitResult commitSemanticPropertiesMutation(
            Map<String, ByteString> semanticPropertiesMap) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation
                .Builder mutationBuilder = new org.chromium.chrome.browser.feed.library.api.host
                                                   .storage.ContentMutation.Builder();
        for (Map.Entry<String, ByteString> entry : semanticPropertiesMap.entrySet()) {
            mutationBuilder.upsert(
                    SEMANTIC_PROPERTIES_PREFIX + entry.getKey(), entry.getValue().toByteArray());
        }
        CommitResult commitResult = mContentStorageDirect.commit(mutationBuilder.build());
        tracker.stop("task", "commitSemanticPropertiesMutation", "mutations",
                semanticPropertiesMap.size());
        return commitResult;
    }

    private Boolean commitSessionMutation(
            String sessionId, List<StreamStructure> streamStructures) {
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);
        JournalMutation.Builder mutation = new JournalMutation.Builder(sessionId);
        if (streamStructures.isEmpty()) {
            // allow an empty journal to be created
            mutation.append(new byte[0]);
        }
        for (StreamStructure streamStructure : streamStructures) {
            mutation.append(streamStructure.toByteArray());
        }
        CommitResult mutationResult = mJournalStorageDirect.commit(mutation.build());
        boolean result = CommitResult.SUCCESS.equals(mutationResult);
        tracker.stop("", "commitSessionMutation", "mutations", streamStructures.size());
        Logger.i(TAG, "commitSessionMutation - Success %s, Update Session %s, stream structures %s",
                result, sessionId, streamStructures.size());
        return result;
    }

    private CommitResult commitContentMutation(List<PayloadWithId> mutations) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);

        org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation
                .Builder contentMutationBuilder = new org.chromium.chrome.browser.feed.library.api
                                                          .host.storage.ContentMutation.Builder();
        for (PayloadWithId mutation : mutations) {
            String payloadId = mutation.contentId;
            StreamPayload payload = mutation.payload;
            if (mutation.payload.hasStreamSharedState()) {
                StreamSharedState streamSharedState = mutation.payload.getStreamSharedState();
                contentMutationBuilder.upsert(
                        SHARED_STATE_PREFIX + streamSharedState.getContentId(),
                        streamSharedState.toByteArray());
            } else {
                contentMutationBuilder.upsert(payloadId, payload.toByteArray());
            }
        }

        // Block waiting for the response from storage, to make this method synchronous.
        // TODO: handle errors
        CommitResult commitResult = mContentStorageDirect.commit(contentMutationBuilder.build());
        tracker.stop("task", "commitContentMutation", "mutations", mutations.size());
        return commitResult;
    }

    private CommitResult commitLocalActionMutation(Map<Integer, List<String>> actions) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);

        CommitResult commitResult = CommitResult.SUCCESS;
        for (Map.Entry<Integer, List<String>> entry : actions.entrySet()) {
            Integer actionType = entry.getKey();
            String journalName;
            if (ActionType.DISMISS == entry.getKey()) {
                journalName = DISMISS_ACTION_JOURNAL;
            } else {
                Logger.e(TAG, "Unknown journal name for action type %s", actionType);
                continue;
            }
            JournalMutation.Builder builder = new JournalMutation.Builder(journalName);
            for (String contentId : entry.getValue()) {
                StreamLocalAction action =
                        StreamLocalAction.newBuilder()
                                .setAction(actionType)
                                .setFeatureContentId(contentId)
                                .setTimestampSeconds(
                                        TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis()))
                                .build();
                byte[] actionBytes = action.toByteArray();
                Logger.i(TAG, "Adding StreamLocalAction bytes %s (length %d) to journal %s",
                        Base64.encodeToString(actionBytes, Base64.DEFAULT), actionBytes.length,
                        journalName);
                builder.append(actionBytes);
            }
            commitResult = mJournalStorageDirect.commit(builder.build());
            if (commitResult == CommitResult.FAILURE) {
                Logger.e(TAG, "Error committing action for type %s", actionType);
                break;
            }
        }

        tracker.stop("task", "commitLocalActionMutation", "actions", actions.size());
        return commitResult;
    }

    private CommitResult commitUploadableActionMutation(
            Map<String, FeedUploadableActionMutation.FeedUploadableActionChanges> actions) {
        ElapsedTimeTracker tracker = mTimingUtils.getElapsedTimeTracker(TAG);

        org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation
                .Builder contentMutationBuilder = new org.chromium.chrome.browser.feed.library.api
                                                          .host.storage.ContentMutation.Builder();
        for (Map.Entry<String, FeedUploadableActionMutation.FeedUploadableActionChanges> entry :
                actions.entrySet()) {
            String contentId = entry.getKey();
            for (StreamUploadableAction removeAction : entry.getValue().removeActions()) {
                contentMutationBuilder.delete(UPLOADABLE_ACTION_PREFIX + contentId
                        + removeAction.getPayload().hashCode());
            }
            for (StreamUploadableAction upsertAction : entry.getValue().upsertActions()) {
                contentMutationBuilder.upsert(
                        UPLOADABLE_ACTION_PREFIX + contentId + upsertAction.getPayload().hashCode(),
                        upsertAction.toByteArray());
            }
        }

        CommitResult commitResult = mContentStorageDirect.commit(contentMutationBuilder.build());
        tracker.stop("task", "commitUploadableActionMutation", "actions", actions.size());
        return commitResult;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        if (mContentStorageDirect instanceof Dumpable) {
            dumper.dump((Dumpable) mContentStorageDirect);
        } else {
            dumper.forKey("contentStorageDirect").value("not dumpable");
        }
        if (mJournalStorageDirect instanceof Dumpable) {
            dumper.dump((Dumpable) mJournalStorageDirect);
        } else {
            dumper.forKey("journalStorage").value("not dumpable");
        }
        dumper.forKey("contentIdStringInternerSize")
                .value(mContentIdStringInterner.size())
                .compactPrevious();
        dumper.forKey("contentIdStringInternerStats")
                .value(mContentIdStringInterner.getStats())
                .compactPrevious();
    }

    /**
     * Clears contents and journals relating to the stream. Leaves actions and semantic properties
     * intact.
     *
     * @return {@code true} when the clear succeeded
     */
    public boolean clearNonActionContent() {
        return clearContentStorage() && clearJournalStorage();
    }

    /**
     * Cleans out journal storage (excluding dismiss actions journal)
     *
     * @return whether the clear succeeded
     */
    private boolean clearJournalStorage() {
        Result<List<String>> allJournalsResult = mJournalStorageDirect.getAllJournals();
        if (!allJournalsResult.isSuccessful()) {
            Logger.e(TAG, "Error clearing all contents. Could not fetch all journals.");
            return false;
        }

        for (String journal : allJournalsResult.getValue()) {
            if (DISMISS_ACTION_JOURNAL.equals(journal)) {
                continue;
            }
            CommitResult result = mJournalStorageDirect.commit(
                    new JournalMutation.Builder(journal).delete().build());
            if (result != CommitResult.SUCCESS) {
                Logger.e(TAG, "Error clearing all contents. Could not delete journal %s", journal);
                return false;
            }
        }
        return true;
    }

    /**
     * Cleans out content storage (excluding semantic properties)
     *
     * @return whether the clear succeeded
     */
    private boolean clearContentStorage() {
        Result<List<String>> results = mContentStorageDirect.getAllKeys();
        if (!results.isSuccessful()) {
            Logger.e(TAG, "Error clearing all contents. Could not fetch all content.");
            return false;
        }
        org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation
                .Builder contentMutationBuilder = new org.chromium.chrome.browser.feed.library.api
                                                          .host.storage.ContentMutation.Builder();
        for (String key : results.getValue()) {
            if (key.contains(SEMANTIC_PROPERTIES_PREFIX)) {
                continue;
            }
            contentMutationBuilder.delete(key);
        }
        CommitResult result = mContentStorageDirect.commit(contentMutationBuilder.build());
        if (result != CommitResult.SUCCESS) {
            Logger.e(TAG, "Error clearing all contents. Could not commit content deletions.");
            return false;
        }
        return true;
    }

    /** Wipes all content and journals */
    @Override
    public boolean clearAll() {
        mThreadUtils.checkNotMainThread();

        boolean success = true;
        // Run clear on both content and journal
        CommitResult result = mContentStorageDirect.commit(
                new org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation
                        .Builder()
                        .deleteAll()
                        .build());
        CommitResult journalResult = mJournalStorageDirect.deleteAll();

        // Confirm results were successful
        if (result != CommitResult.SUCCESS) {
            Logger.e(TAG, "Error clearing all. Could not delete all content from content storage.");
            success = false;
        }

        if (journalResult != CommitResult.SUCCESS) {
            Logger.e(
                    TAG, "Error clearing all. Could not delete all journals from journal storage.");
            success = false;
        }

        // Try to recreate $HEAD (it needs to exist in order to create new sessions)
        // #clearHead() also will create it if it does not exist
        clearHead();

        return success;
    }
}
