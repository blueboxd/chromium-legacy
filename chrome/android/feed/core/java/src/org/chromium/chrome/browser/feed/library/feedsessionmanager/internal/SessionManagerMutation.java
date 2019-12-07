// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.logging.StringFormattingUtils;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Class which implements factory to create the task which commits a mutation to the
 * FeedSessionManager and Sessions. This is created and used by the {@link
 * org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager} to
 * commit a mutation defined as a List of {@link StreamDataOperation}.
 */
public final class SessionManagerMutation implements Dumpable {
    private static final String TAG = "SessionManagerMutation";

    private final Store mStore;
    private final SessionCache mSessionCache;
    private final ContentCache mContentCache;
    private final TaskQueue mTaskQueue;
    private final SchedulerApi mSchedulerApi;
    private final ThreadUtils mThreadUtils;
    private final TimingUtils mTimingUtils;
    private final Clock mClock;
    private final MainThreadRunner mMainThreadRunner;
    private final BasicLoggingApi mBasicLoggingApi;

    // operation counts for the dumper
    private int mCreateCount;
    private int mCommitCount;
    private int mErrorCount;
    private int mContentCommitErrorCount;
    private int mSemanticPropertiesCommitErrorCount;

    /** Listens for errors which need to be reported to a ModelProvider. */
    public interface ModelErrorObserver {
        void onError(/*@Nullable*/ Session session, ModelError error);
    }

    public SessionManagerMutation(Store store, SessionCache sessionCache, ContentCache contentCache,
            TaskQueue taskQueue, SchedulerApi schedulerApi, ThreadUtils threadUtils,
            TimingUtils timingUtils, Clock clock, MainThreadRunner mainThreadRunner,
            BasicLoggingApi basicLoggingApi) {
        this.mStore = store;
        this.mSessionCache = sessionCache;
        this.mContentCache = contentCache;
        this.mTaskQueue = taskQueue;
        this.mSchedulerApi = schedulerApi;
        this.mThreadUtils = threadUtils;
        this.mTimingUtils = timingUtils;
        this.mClock = clock;
        this.mMainThreadRunner = mainThreadRunner;
        this.mBasicLoggingApi = basicLoggingApi;
    }

    /**
     * Return a Consumer of StreamDataOperations which will update the {@link
     * org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager}.
     */
    public Consumer<Result<Model>> createCommitter(String task, MutationContext mutationContext,
            ModelErrorObserver modelErrorObserver,
            KnownContent./*@Nullable*/ Listener knownContentListener) {
        mCreateCount++;
        return new MutationCommitter(task, mutationContext, modelErrorObserver,
                knownContentListener, mMainThreadRunner, mBasicLoggingApi);
    }

    public void resetHead() {
        HeadMutationCommitter mutation = new HeadMutationCommitter();
        mTaskQueue.execute(
                Task.INVALIDATE_HEAD, TaskType.HEAD_INVALIDATE, () -> mutation.resetHead(null));
    }

    public void forceResetHead() {
        HeadMutationCommitter mutation = new HeadMutationCommitter();
        mutation.resetHead(null);
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("mutationsCreated").value(mCreateCount);
        dumper.forKey("commitCount").value(mCommitCount).compactPrevious();
        dumper.forKey("errorCount").value(mErrorCount).compactPrevious();
        dumper.forKey("contentCommitErrorCount").value(mContentCommitErrorCount).compactPrevious();
        dumper.forKey("semanticPropertiesCommitErrorCount")
                .value(mSemanticPropertiesCommitErrorCount)
                .compactPrevious();
    }

    public static boolean validDataOperation(StreamDataOperation dataOperation) {
        if (!dataOperation.hasStreamPayload() || !dataOperation.hasStreamStructure()) {
            Logger.e(TAG, "Invalid StreamDataOperation - payload or streamStructure not defined");
            return false;
        }
        String contentId = dataOperation.getStreamStructure().getContentId();
        if (TextUtils.isEmpty(contentId)) {
            Logger.e(TAG, "Invalid StreamDataOperation - No ID Found");
            return false;
        }
        return true;
    }

    // TODO: Tiktok doesn't allow MutationCommitter to extend HeadMutationCommitter
    class HeadMutationCommitter {
        /**
         * This runs as a task on the executor thread and also as part of a SessionMutation commit.
         */
        @VisibleForTesting
        void resetHead(/*@Nullable*/ String mutationSessionId) {
            mThreadUtils.checkNotMainThread();
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
            Collection<Session> attachedSessions = mSessionCache.getAttachedSessions();

            // If we have old sessions and we received a clear head, let's invalidate the session
            // that initiated the clear.
            mStore.clearHead();
            for (Session session : attachedSessions) {
                ModelProvider modelProvider = session.getModelProvider();
                if (modelProvider != null && session.invalidateOnResetHead()
                        && shouldInvalidateSession(mutationSessionId, modelProvider)) {
                    invalidateSession(modelProvider, session);
                }
            }
            timeTracker.stop("task", "resetHead");
        }

        /**
         * This will determine if the ModelProvider (session) should be invalidated.
         *
         * <ol>
         *   <li>ModelProvider should be READY
         *   <li>Clearing head was initiated externally, not from an existing session
         *   <li>Clearing head was initiated by the ModelProvider
         *   <li>The ModelProvider doesn't yet have a session, so we'll create the session from the
         * new $HEAD
         * </ol>
         */
        @VisibleForTesting
        boolean shouldInvalidateSession(
                /*@Nullable*/ String mutationSessionId, ModelProvider modelProvider) {
            // if the model provider isn't READY, don't invalidate the session
            if (modelProvider.getCurrentState() != State.READY) {
                return false;
            }
            // Clear was done outside of any session.
            if (mutationSessionId == null) {
                return true;
            }
            // Invalidate if the ModelProvider doesn't yet have a session or if it matches the
            // session which initiated the request clearing $HEAD.
            String sessionId = modelProvider.getSessionId();
            return sessionId == null || sessionId.equals(mutationSessionId);
        }

        private void invalidateSession(ModelProvider modelProvider, Session session) {
            mThreadUtils.checkNotMainThread();
            Logger.i(TAG, "Invalidate session %s", session.getSessionId());
            modelProvider.invalidate();
        }
    }

    @VisibleForTesting
    class MutationCommitter extends HeadMutationCommitter implements Consumer<Result<Model>> {
        private final String mTask;
        private final MutationContext mMutationContext;
        private final ModelErrorObserver mModelErrorObserver;
        private final KnownContent./*@Nullable*/ Listener mKnownContentListener;

        private final List<StreamStructure> mStreamStructures = new ArrayList<>();
        private final MainThreadRunner mMainThreadRunner;
        private final BasicLoggingApi mBasicLoggingApi;

        @VisibleForTesting
        boolean mClearedHead;
        private Model mModel;

        private MutationCommitter(String task, MutationContext mutationContext,
                ModelErrorObserver modelErrorObserver,
                KnownContent./*@Nullable*/ Listener knownContentListener,
                MainThreadRunner mainThreadRunner, BasicLoggingApi basicLoggingApi) {
            this.mTask = task;
            this.mMutationContext = mutationContext;
            this.mModelErrorObserver = modelErrorObserver;
            this.mKnownContentListener = knownContentListener;
            this.mMainThreadRunner = mainThreadRunner;
            this.mBasicLoggingApi = basicLoggingApi;
        }

        @Override
        public void accept(Result<Model> updateResults) {
            if (!updateResults.isSuccessful()) {
                mErrorCount++;
                Session session = null;
                String sessionId = mMutationContext.getRequestingSessionId();
                if (sessionId != null) {
                    session = mSessionCache.getAttached(sessionId);
                }
                if (mMutationContext.getContinuationToken() != null) {
                    StreamToken streamToken = mMutationContext.getContinuationToken();
                    if (session != null && streamToken != null) {
                        Logger.e(TAG, "Error found with a token request %s",
                                streamToken.getContentId());
                        mModelErrorObserver.onError(session,
                                new ModelError(ErrorType.PAGINATION_ERROR,
                                        streamToken.getNextPageToken()));
                    } else {
                        Logger.e(TAG, "Unable to process PAGINATION_ERROR");
                    }
                } else {
                    Logger.e(TAG, "Update error, the update is being ignored");
                    mModelErrorObserver.onError(
                            session, new ModelError(ErrorType.NO_CARDS_ERROR, null));
                    mTaskQueue.execute(Task.REQUEST_FAILURE, TaskType.HEAD_RESET, () -> {});
                }
                return;
            }
            mModel = updateResults.getValue();
            for (StreamDataOperation operation : mModel.streamDataOperations) {
                if (operation.getStreamStructure().getOperation() == Operation.CLEAR_ALL) {
                    mClearedHead = true;
                    break;
                }
            }
            int taskType;
            if (mMutationContext != null && mMutationContext.isUserInitiated()) {
                taskType = TaskType.IMMEDIATE;
            } else {
                taskType = mClearedHead ? TaskType.HEAD_RESET : TaskType.USER_FACING;
            }
            mTaskQueue.execute(Task.COMMIT_TASK, taskType, this::commitTask);
        }

        private void commitTask() {
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
            commitContent();
            commitSessionUpdates();
            mCommitCount++;
            timeTracker.stop("task", "sessionMutation.commitTask:" + mTask, "mutations",
                    mStreamStructures.size(), "userInitiated",
                    mMutationContext != null ? mMutationContext.isUserInitiated()
                                             : "No MutationContext");
        }

        private void commitContent() {
            mThreadUtils.checkNotMainThread();
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);

            mContentCache.startMutation();
            ContentMutation contentMutation = mStore.editContent();
            SemanticPropertiesMutation semanticPropertiesMutation = mStore.editSemanticProperties();
            for (StreamDataOperation dataOperation : mModel.streamDataOperations) {
                Operation operation = dataOperation.getStreamStructure().getOperation();
                if (operation == Operation.CLEAR_ALL) {
                    mStreamStructures.add(dataOperation.getStreamStructure());
                    resetHead(mMutationContext.getRequestingSessionId());
                    continue;
                }

                if (operation == Operation.UPDATE_OR_APPEND) {
                    if (!validDataOperation(dataOperation)) {
                        mErrorCount++;
                        continue;
                    }
                    String contentId = dataOperation.getStreamStructure().getContentId();
                    StreamPayload payload = dataOperation.getStreamPayload();
                    if (payload.hasStreamSharedState()) {
                        // don't add StreamSharedState to the metadata list stored for sessions
                        contentMutation.add(contentId, payload);
                    } else if (payload.hasStreamFeature() || payload.hasStreamToken()) {
                        mContentCache.put(contentId, payload);
                        contentMutation.add(contentId, payload);
                        mStreamStructures.add(dataOperation.getStreamStructure());
                    } else if (dataOperation.getStreamPayload().hasSemanticData()) {
                        semanticPropertiesMutation.add(
                                contentId, dataOperation.getStreamPayload().getSemanticData());
                    } else {
                        Logger.e(TAG, "Unsupported UPDATE_OR_APPEND payload");
                    }
                    continue;
                }

                if (operation == Operation.REMOVE) {
                    // We don't update the content for REMOVED items, content will be garbage
                    // collected.
                    mStreamStructures.add(dataOperation.getStreamStructure());
                    continue;
                }

                if (operation == Operation.REQUIRED_CONTENT) {
                    mStreamStructures.add(dataOperation.getStreamStructure());
                    continue;
                }

                mErrorCount++;
                Logger.e(TAG, "Unsupported Mutation: %s",
                        dataOperation.getStreamStructure().getOperation());
            }
            mTaskQueue.execute(Task.PERSIST_MUTATION, TaskType.BACKGROUND, () -> {
                if (contentMutation.commit().getResult() == CommitResult.Result.FAILURE) {
                    mContentCommitErrorCount++;
                    Logger.e(TAG, "contentMutation failed");
                    mMainThreadRunner.execute("CONTENT_MUTATION_FAILED", () -> {
                        mBasicLoggingApi.onInternalError(InternalFeedError.CONTENT_MUTATION_FAILED);
                    });
                }
                if (semanticPropertiesMutation.commit().getResult()
                        == CommitResult.Result.FAILURE) {
                    mSemanticPropertiesCommitErrorCount++;
                    Logger.e(TAG, "semanticPropertiesMutation failed");
                }
                mContentCache.finishMutation();
            });
            timeTracker.stop("", "contentUpdate", "items", mModel.streamDataOperations.size());
        }

        private void commitSessionUpdates() {
            mThreadUtils.checkNotMainThread();
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);

            StreamDataProto.StreamToken mutationSourceToken =
                    (mMutationContext != null) ? mMutationContext.getContinuationToken() : null;
            // For sessions we want to add the remove operation if the mutation source was a
            // continuation token.
            if (mutationSourceToken != null) {
                StreamStructure removeOperation = addTokenRemoveOperation(mutationSourceToken);
                if (removeOperation != null) {
                    mStreamStructures.add(0, removeOperation);
                }
            }
            Collection<Session> updates = mSessionCache.getAllSessions();

            HeadSessionImpl head = mSessionCache.getHead();
            for (Session session : updates) {
                ModelProvider modelProvider = session.getModelProvider();
                if (modelProvider != null && modelProvider.getCurrentState() == State.INVALIDATED) {
                    Logger.w(TAG, "Removing an invalidate session");
                    // Remove all invalidated sessions
                    mSessionCache.removeAttached(session.getSessionId());
                    continue;
                }
                if (session == head) {
                    long updateTime = mClock.currentTimeMillis();
                    if (mClearedHead) {
                        session.updateSession(mClearedHead, mStreamStructures, mModel.schemaVersion,
                                mMutationContext);
                        mSessionCache.updateHeadMetadata(updateTime, mModel.schemaVersion);
                        mSchedulerApi.onReceiveNewContent(updateTime);
                        if (mKnownContentListener != null) {
                            mKnownContentListener.onNewContentReceived(
                                    /* isNewRefresh */ true, updateTime);
                        }
                        Logger.i(TAG, "Cleared Head, new creation time %s",
                                StringFormattingUtils.formatLogDate(updateTime));
                        continue;
                    } else if (mKnownContentListener != null) {
                        mKnownContentListener.onNewContentReceived(
                                /* isNewRefresh */ false, updateTime);
                    }
                }
                Logger.i(TAG, "Update Session %s", session.getSessionId());
                session.updateSession(
                        mClearedHead, mStreamStructures, mModel.schemaVersion, mMutationContext);
            }
            timeTracker.stop("", "sessionUpdate", "sessions", updates.size());
        }

        /*@Nullable*/
        private StreamStructure addTokenRemoveOperation(StreamToken token) {
            return StreamStructure.newBuilder()
                    .setContentId(token.getContentId())
                    .setParentContentId(token.getParentId())
                    .setOperation(Operation.REMOVE)
                    .build();
        }
    }
}
