// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider;

import android.support.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelMutation;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.RemoveTracking;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompleted;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.chrome.browser.feed.library.common.functional.Predicate;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.CursorProvider;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.FeatureChangeImpl;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.ModelChildBinder;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.ModelCursorImpl;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.ModelMutationImpl;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.ModelMutationImpl.Change;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.UpdatableModelChild;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.UpdatableModelFeature;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.UpdatableModelToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.UUID;

import javax.annotation.concurrent.GuardedBy;

/** An implementation of {@link ModelProvider}. This will represent the Stream tree in memory. */
public final class FeedModelProvider
        extends FeedObservable<ModelProviderObserver> implements ModelProvider, Dumpable {
    private static final String TAG = "FeedModelProvider";
    private static final List<UpdatableModelChild> EMPTY_LIST =
            Collections.unmodifiableList(new ArrayList<>());
    private static final String SYNTHETIC_TOKEN_PREFIX = "_token:";

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    /*@Nullable*/
    private UpdatableModelChild mRoot;

    // The tree is model as a parent with an list of children.  A container is created for every
    // ModelChild with a child.
    @GuardedBy("mLock")
    private final Map<String, ArrayList<UpdatableModelChild>> mContainers = new HashMap<>();

    @GuardedBy("mLock")
    private final Map<String, UpdatableModelChild> mContents = new HashMap<>();

    @GuardedBy("mLock")
    private final Map<ByteString, TokenTracking> mTokens = new HashMap<>();

    @GuardedBy("mLock")
    private final Map<String, SyntheticTokenTracker> mSyntheticTokens = new HashMap<>();

    // TODO: Tiktok doesn't like WeakReference and will report uses as conformance errors
    @GuardedBy("mLock")
    private final List<WeakReference<ModelCursorImpl>> mCursors = new ArrayList<>();

    @GuardedBy("mLock")
    private ModelState mCurrentState = ModelState.initializing();

    // #dump() operation counts
    private int mRemovedChildrenCount;
    private int mRemoveScanCount;
    private int mCommitCount;
    private int mCommitTokenCount;
    private int mCommitUpdateCount;
    private int mCursorsRemoved;

    private final FeedSessionManager mFeedSessionManager;
    private final ThreadUtils mThreadUtils;
    private final TaskQueue mTaskQueue;
    private final MainThreadRunner mMainThreadRunner;
    private final ModelChildBinder mModelChildBinder;
    private final TimingUtils mTimingUtils;
    private final BasicLoggingApi mBasicLoggingApi;

    /*@Nullable*/ private final Predicate<StreamStructure> mFilterPredicate;
    /*@Nullable*/ private RemoveTrackingFactory<?> mRemoveTrackingFactory;

    private final int mInitialPageSize;
    private final int mPageSize;
    private final int mMinPageSize;

    @VisibleForTesting /*@Nullable*/ String mSessionId;

    @GuardedBy("mLock")
    private boolean mDelayedTriggerRefresh;

    @GuardedBy("mLock")
    @RequestReason
    private int mRequestReason = RequestReason.UNKNOWN;

    FeedModelProvider(FeedSessionManager feedSessionManager, ThreadUtils threadUtils,
            TimingUtils timingUtils, TaskQueue taskQueue, MainThreadRunner mainThreadRunner,
            /*@Nullable*/ Predicate<StreamStructure> filterPredicate, Configuration config,
            BasicLoggingApi basicLoggingApi) {
        this.mFeedSessionManager = feedSessionManager;
        this.mThreadUtils = threadUtils;
        this.mTimingUtils = timingUtils;
        this.mTaskQueue = taskQueue;
        this.mMainThreadRunner = mainThreadRunner;
        this.mInitialPageSize =
                (int) config.getValueOrDefault(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, 0L);
        this.mPageSize = (int) config.getValueOrDefault(ConfigKey.NON_CACHED_PAGE_SIZE, 0L);
        this.mMinPageSize = (int) config.getValueOrDefault(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, 0L);
        this.mFilterPredicate = filterPredicate;
        this.mBasicLoggingApi = basicLoggingApi;

        CursorProvider cursorProvider = parentId -> provideCursor(parentId);
        mModelChildBinder = new ModelChildBinder(feedSessionManager, cursorProvider, timingUtils);
    }

    private ModelCursorImpl provideCursor(String parentId) {
        synchronized (mLock) {
            ArrayList<UpdatableModelChild> children = mContainers.get(parentId);
            if (children == null) {
                Logger.i(TAG, "No children found for Cursor");
                ModelCursorImpl cursor = new ModelCursorImpl(parentId, EMPTY_LIST);
                mCursors.add(new WeakReference<>(cursor));
                return cursor;
            }
            ModelCursorImpl cursor = new ModelCursorImpl(parentId, new ArrayList<>(children));
            mCursors.add(new WeakReference<>(cursor));
            return cursor;
        }
    }

    @Override
    /*@Nullable*/
    public ModelFeature getRootFeature() {
        synchronized (mLock) {
            UpdatableModelChild localRoot = mRoot;
            if (localRoot == null) {
                Logger.i(TAG, "Found Empty Stream");
                return null;
            }
            if (localRoot.getType() != Type.FEATURE) {
                mBasicLoggingApi.onInternalError(InternalFeedError.ROOT_NOT_BOUND_TO_FEATURE);
                Logger.e(TAG, "Root is bound to the wrong type %s", localRoot.getType());
                return null;
            }
            return localRoot.getModelFeature();
        }
    }

    @Override
    /*@Nullable*/
    public ModelChild getModelChild(String contentId) {
        synchronized (mLock) {
            return mContents.get(contentId);
        }
    }

    @Override
    /*@Nullable*/
    public StreamSharedState getSharedState(ContentId contentId) {
        return mFeedSessionManager.getSharedState(contentId);
    }

    @Override
    public boolean handleToken(ModelToken modelToken) {
        if (modelToken instanceof UpdatableModelToken) {
            UpdatableModelToken token = (UpdatableModelToken) modelToken;
            if (token.isSynthetic()) {
                SyntheticTokenTracker tokenTracker;
                synchronized (mLock) {
                    tokenTracker = mSyntheticTokens.get(token.getStreamToken().getContentId());
                }
                if (tokenTracker == null) {
                    Logger.e(TAG, "Unable to find the SyntheticTokenTracker");
                    return false;
                }
                // The nullness checker fails to understand tokenTracker can't be null in the Lambda
                // usage
                SyntheticTokenTracker tt = Validators.checkNotNull(tokenTracker);
                mTaskQueue.execute(Task.HANDLE_SYNTHETIC_TOKEN, TaskType.USER_FACING,
                        () -> tt.handleSyntheticToken(token));
                return true;
            }
        }
        String sessionId = Validators.checkNotNull(this.mSessionId);
        mFeedSessionManager.handleToken(sessionId, modelToken.getStreamToken());
        return true;
    }

    @Override
    public void triggerRefresh(@RequestReason int requestReason) {
        triggerRefresh(requestReason, UiContext.getDefaultInstance());
    }

    @Override
    public void triggerRefresh(@RequestReason int requestReason, UiContext uiContext) {
        mThreadUtils.checkMainThread();
        if (mSessionId == null) {
            synchronized (mLock) {
                mDelayedTriggerRefresh = true;
                this.mRequestReason = requestReason;
            }
            return;
        }
        mFeedSessionManager.triggerRefresh(mSessionId, requestReason, uiContext);
    }

    @Override
    public void registerObserver(ModelProviderObserver observer) {
        super.registerObserver(observer);
        synchronized (mLock) {
            // If we are in the ready state, then call the Observer to inform it things are ready.
            if (mCurrentState.isReady()) {
                observer.onSessionStart(mCurrentState.mUiContext);
            } else if (mCurrentState.isInvalidated()) {
                observer.onSessionFinished(mCurrentState.mUiContext);
            }
        }
    }

    @Override
    public @State int getCurrentState() {
        synchronized (mLock) {
            return mCurrentState.getState();
        }
    }

    @Override
    /*@Nullable*/
    public String getSessionId() {
        if (mSessionId == null) {
            Logger.w(TAG, "sessionId is null, this should have been set during population");
        }
        return mSessionId;
    }

    @Override
    public List<ModelChild> getAllRootChildren() {
        synchronized (mLock) {
            if (mRoot == null) {
                return Collections.emptyList();
            }
            List<UpdatableModelChild> rootChildren = mContainers.get(mRoot.getContentId());
            return (rootChildren != null) ? new ArrayList<>(rootChildren) : Collections.emptyList();
        }
    }

    @Override
    public void enableRemoveTracking(RemoveTrackingFactory<?> removeTrackingFactory) {
        this.mRemoveTrackingFactory = removeTrackingFactory;
    }

    @Override
    public ModelMutation edit() {
        return new ModelMutationImpl(mCommitter);
    }

    @Override
    public void detachModelProvider() {
        if (!moveToInvalidateState(UiContext.getDefaultInstance())) {
            Logger.e(TAG, "unable to detach FeedModelProvider");
            return;
        }
        String sessionId = getSessionId();
        if (sessionId != null) {
            Logger.i(TAG, "Detach the current ModelProvider: session %s", sessionId);
            mFeedSessionManager.detachSession(sessionId);
        }
    }

    @Override
    public void invalidate() {
        invalidate(UiContext.getDefaultInstance());
    }

    @Override
    public void invalidate(UiContext uiContext) {
        if (!moveToInvalidateState(uiContext)) {
            Logger.e(TAG, "unable to invalidate FeedModelProvider");
            return;
        }
        String sessionId = getSessionId();
        if (sessionId != null) {
            Logger.i(TAG, "Invalidating the current ModelProvider: session %s", sessionId);
            mFeedSessionManager.invalidateSession(sessionId);
        }

        // Always run the observers on the UI Thread
        mMainThreadRunner.execute(TAG + " onSessionFinished", () -> {
            List<ModelProviderObserver> observerList = getObserversToNotify();
            for (ModelProviderObserver observer : observerList) {
                observer.onSessionFinished(uiContext);
            }
        });
    }

    private boolean moveToInvalidateState(UiContext uiContext) {
        synchronized (mLock) {
            if (mCurrentState.isInvalidated()) {
                Logger.i(TAG, "Invalidated an already invalid ModelProvider");
                return false;
            }
            Logger.i(TAG, "Moving %s to INVALIDATED",
                    mSessionId != null ? mSessionId : "No sessionId");
            mCurrentState = ModelState.invalidated(uiContext);
            for (WeakReference<ModelCursorImpl> cursorRef : mCursors) {
                ModelCursorImpl cursor = cursorRef.get();
                if (cursor != null) {
                    cursor.release();
                    cursorRef.clear();
                }
            }
            mCursors.clear();
            mTokens.clear();
            mSyntheticTokens.clear();
            mContainers.clear();
        }
        return true;
    }

    @Override
    public void raiseError(ModelError error) {
        if (error.getErrorType() == ErrorType.NO_CARDS_ERROR) {
            mMainThreadRunner.execute(TAG + " onError", () -> {
                List<ModelProviderObserver> observerList = getObserversToNotify();
                for (ModelProviderObserver observer : observerList) {
                    observer.onError(error);
                }
            });
        } else if (error.getErrorType() == ErrorType.PAGINATION_ERROR) {
            Logger.i(TAG, "handling Pagination error");
            TokenTracking token;
            synchronized (mLock) {
                token = mTokens.get(error.getContinuationToken());
            }
            if (token != null) {
                raiseErrorOnToken(error, token.mTokenChild);
            } else {
                Logger.e(TAG, "The Token Observer was not found during pagination error");
            }
        }
    }

    private void raiseErrorOnToken(ModelError error, UpdatableModelToken token) {
        mMainThreadRunner.execute(TAG + " onTokenChange", () -> {
            List<TokenCompletedObserver> observerList = token.getObserversToNotify();
            for (TokenCompletedObserver observer : observerList) {
                observer.onError(error);
            }
        });
    }

    /**
     * This wraps the ViewDepthProvider provided by the UI. It does this so it can verify that the
     * returned contentId is one with the Root as the parent.
     */
    /*@Nullable*/
    ViewDepthProvider getViewDepthProvider(/*@Nullable*/ ViewDepthProvider delegate) {
        if (delegate == null) {
            return null;
        }
        return new ViewDepthProvider() {
            @Override
            public /*@Nullable*/ String getChildViewDepth() {
                String cid = Validators.checkNotNull(delegate).getChildViewDepth();
                synchronized (mLock) {
                    if (cid == null || mRoot == null) {
                        return null;
                    }
                    String rootId = mRoot.getContentId();
                    UpdatableModelChild child = mContents.get(cid);
                    while (child != null) {
                        if (child.getParentId() == null) {
                            return null;
                        }
                        if (rootId.equals(child.getParentId())) {
                            return child.getContentId();
                        }
                        child = mContents.get(child.getParentId());
                    }
                }
                return null;
            }
        };
    }

    @Override
    public void dump(Dumper dumper) {
        synchronized (mLock) {
            dumper.title(TAG);
            dumper.forKey("currentState").value(mCurrentState.getState());
            dumper.forKey("contentCount").value(mContents.size()).compactPrevious();
            dumper.forKey("containers").value(mContainers.size()).compactPrevious();
            dumper.forKey("tokens").value(mTokens.size()).compactPrevious();
            dumper.forKey("syntheticTokens").value(mSyntheticTokens.size()).compactPrevious();
            dumper.forKey("observers").value(mObservers.size()).compactPrevious();
            dumper.forKey("commitCount").value(mCommitCount);
            dumper.forKey("commitTokenCount").value(mCommitTokenCount).compactPrevious();
            dumper.forKey("commitUpdateCount").value(mCommitUpdateCount).compactPrevious();
            dumper.forKey("removeCount").value(mRemovedChildrenCount);
            dumper.forKey("removeScanCount").value(mRemoveScanCount).compactPrevious();
            if (mRoot != null) {
                // This is here to satisfy the nullness checker.
                UpdatableModelChild nonNullRoot = Validators.checkNotNull(mRoot);
                if (nonNullRoot.getType() != Type.FEATURE) {
                    dumper.forKey("root").value("[ROOT NOT A FEATURE]");
                    dumper.forKey("type").value(nonNullRoot.getType()).compactPrevious();
                } else if (nonNullRoot.getModelFeature().getStreamFeature() != null
                        && nonNullRoot.getModelFeature().getStreamFeature().hasContentId()) {
                    dumper.forKey("root").value(
                            nonNullRoot.getModelFeature().getStreamFeature().getContentId());
                } else {
                    dumper.forKey("root").value("[FEATURE NOT DEFINED]");
                }
            } else {
                dumper.forKey("root").value("[UNDEFINED]");
            }
            int singleChild = 0;
            Dumper childDumper = dumper.getChildDumper();
            childDumper.title("Containers With Multiple Children");
            for (Entry<String, ArrayList<UpdatableModelChild>> entry : mContainers.entrySet()) {
                if (entry.getValue().size() > 1) {
                    childDumper.forKey("Container").value(entry.getKey());
                    childDumper.forKey("childrenCount")
                            .value(entry.getValue().size())
                            .compactPrevious();
                } else {
                    singleChild++;
                }
            }
            dumper.forKey("singleChildContainers").value(singleChild);
            dumper.forKey("cursors").value(mCursors.size());
            int atEnd = 0;
            int cursorEmptyRefs = 0;
            for (WeakReference<ModelCursorImpl> cursorRef : mCursors) {
                ModelCursorImpl cursor = cursorRef.get();
                if (cursor == null) {
                    cursorEmptyRefs++;
                } else if (cursor.isAtEnd()) {
                    atEnd++;
                }
            }
            dumper.forKey("cursorsRemoved").value(mCursorsRemoved).compactPrevious();
            dumper.forKey("reclaimedWeakReferences").value(cursorEmptyRefs).compactPrevious();
            dumper.forKey("cursorsAtEnd").value(atEnd).compactPrevious();

            for (WeakReference<ModelCursorImpl> cursorRef : mCursors) {
                ModelCursorImpl cursor = cursorRef.get();
                if (cursor != null && !cursor.isAtEnd()) {
                    dumper.dump(cursor);
                }
            }
        }
    }

    @VisibleForTesting
    List<ModelProviderObserver> getObserversToNotify() {
        // Make a copy of the observers, so the observers are not mutated while invoking callbacks.
        // mObservers is locked when adding or removing observers. Also, release the lock before
        // invoking callbacks to avoid deadlocks. ([INTERNAL LINK])
        synchronized (mObservers) {
            return new ArrayList<>(mObservers);
        }
    }

    /**
     * Abstract class used by the {@code ModelMutatorCommitter} to modify the model state based upon
     * the current model state and the contents of the mutation. We define mutation handlers for the
     * initialization, for a mutation based upon a continuation token response, and then a standard
     * update mutation. The default implementation is a no-op.
     */
    abstract static class MutationHandler {
        private MutationHandler() {}

        /**
         * Called before processing the children of the mutation. This allows the model to be
         * cleaned up before new children are added.
         */
        void preMutation() {}

        /** Append a child to a parent */
        void appendChild(String parentKey, UpdatableModelChild child) {}

        /** Remove a child from a parent */
        void removeChild(String parentKey, UpdatableModelChild child) {}

        /**
         * This is called after the model has been updated. Typically this will notify observers of
         * the changes made during the mutation.
         */
        void postMutation() {}
    }

    /** This is the {@code ModelMutatorCommitter} which updates the model. */
    private final Committer<Void, Change> mCommitter = new Committer<Void, Change>() {
        @Override
        public Void commit(Change change) {
            Logger.i(TAG, "FeedModelProvider - committer, structure changes %s, update changes %s",
                    change.mStructureChanges.size(), change.mUpdateChanges.size());
            mThreadUtils.checkNotMainThread();
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
            mCommitCount++;

            if (change.mSessionId != null) {
                mSessionId = change.mSessionId;
                synchronized (mLock) {
                    if (mDelayedTriggerRefresh) {
                        mDelayedTriggerRefresh = false;
                        final @RequestReason int requestReasonForTrigger = mRequestReason;
                        mMainThreadRunner.execute(TAG + " TriggerRefresh",
                                () -> triggerRefresh(requestReasonForTrigger));
                        mRequestReason = RequestReason.UNKNOWN;
                    }
                }
            }

            // All appends and updates are considered unbound (childrenToBind) and will need to be
            // sent to the ModelChildBinder.
            Map<String, UpdatableModelChild> appendedChildren = new HashMap<>();
            List<UpdatableModelChild> childrenToBind = new ArrayList<>();
            boolean removedChildren = false;
            for (StreamStructure structureChange : change.mStructureChanges) {
                if (mFilterPredicate != null && !mFilterPredicate.test(structureChange)) {
                    continue;
                }
                if (structureChange.getOperation() == Operation.UPDATE_OR_APPEND) {
                    String contentId = structureChange.getContentId();
                    UpdatableModelChild child = new UpdatableModelChild(
                            contentId, structureChange.getParentContentId());
                    appendedChildren.put(contentId, child);
                    childrenToBind.add(child);
                } else if (structureChange.getOperation() == Operation.REMOVE) {
                    removedChildren = true;
                }
            }

            RemoveTracking<?> removeTracking = null;
            if (mRemoveTrackingFactory != null && change.mMutationContext != null
                    && removedChildren) {
                removeTracking = mRemoveTrackingFactory.create(change.mMutationContext);
            }

            synchronized (mLock) {
                // Add the updates to the childrenToBind
                for (StreamStructure updatedChild : change.mUpdateChanges) {
                    UpdatableModelChild child = mContents.get(updatedChild.getContentId());
                    if (child != null) {
                        childrenToBind.add(child);
                    } else {
                        Logger.w(TAG, "child %s was not found for updating",
                                updatedChild.getContentId());
                    }
                }
            }

            // Mutate the Model
            MutationHandler mutationHandler =
                    getMutationHandler(change.mUpdateChanges, change.mMutationContext);
            processMutation(mutationHandler, change.mStructureChanges, appendedChildren,
                    removeTracking, change.mMutationContext);

            if (removeTracking != null) {
                // Update the UI on the main thread.
                mMainThreadRunner.execute(
                        TAG + " removeTracking", removeTracking::triggerConsumerUpdate);
            }

            // Determine where to start binding children.
            int tokenStart = 0;
            synchronized (mLock) {
                if (mRoot != null) {
                    tokenStart = findFirstUnboundChild(mContainers.get(mRoot.getContentId()));
                }
            }
            int tokenPageSize = tokenStart < mInitialPageSize ? mInitialPageSize : mPageSize;

            synchronized (mLock) {
                if (shouldInsertSyntheticToken()) {
                    SyntheticTokenTracker tokenTracker = new SyntheticTokenTracker(
                            Validators.checkNotNull(mRoot), tokenStart, tokenPageSize);
                    childrenToBind = tokenTracker.insertToken();
                }
            }

            boolean success = bindChildrenAndTokens(childrenToBind);
            if (success) {
                mutationHandler.postMutation();
            } else {
                Logger.e(TAG, "bindChildrenAndTokens failed, not processing mutation");
                invalidate();
            }
            timeTracker.stop("", "modelProviderCommit");
            StreamToken token = (change.mMutationContext != null)
                    ? change.mMutationContext.getContinuationToken()
                    : null;
            Logger.i(TAG,
                    "ModelProvider Mutation committed - structure changes %s, childrenToBind %s, "
                            + "removedChildren %s, Token %s",
                    change.mStructureChanges.size(), childrenToBind.size(), removedChildren,
                    token != null);
            return null;
        }

        private int findFirstUnboundChild(/*@Nullable*/ List<UpdatableModelChild> children) {
            int firstUnboundChild = 0;
            if (children != null) {
                firstUnboundChild = children.size() - 1;
                for (int i = 0; i < children.size() - 1; i++) {
                    if (children.get(i).getType() == Type.UNBOUND) {
                        firstUnboundChild = i;
                        break;
                    }
                }
            }
            return firstUnboundChild;
        }

        /** Returns a MutationHandler for processing the mutation */
        private MutationHandler getMutationHandler(List<StreamStructure> updatedChildren,
                /*@Nullable*/ MutationContext mutationContext) {
            synchronized (mLock) {
                StreamToken mutationSourceToken =
                        mutationContext != null ? mutationContext.getContinuationToken() : null;
                MutationHandler mutationHandler;
                if (mCurrentState.isInitializing()) {
                    Validators.checkState(mutationSourceToken == null,
                            "Initializing the Model Provider from a Continuation Token");

                    UiContext uiContext = mutationContext != null ? mutationContext.getUiContext()
                                                                  : UiContext.getDefaultInstance();
                    mutationHandler = new InitializeModel(uiContext);
                } else if (mutationSourceToken != null) {
                    mutationHandler = new TokenMutation(mutationSourceToken);
                    mCommitTokenCount++;
                } else {
                    mutationHandler = new UpdateMutation(updatedChildren);
                    mCommitUpdateCount++;
                }
                return mutationHandler;
            }
        }

        /** Process the structure changes to update the model. */
        void processMutation(MutationHandler mutationHandler,
                List<StreamStructure> structureChanges,
                Map<String, UpdatableModelChild> appendedChildren,
                /*@Nullable*/ RemoveTracking<?> removeTracking,
                /*@Nullable*/ MutationContext mutationContext) {
            ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
            int appends = 0;
            int removes = 0;
            synchronized (mLock) {
                // Processing before the structural mutation
                mutationHandler.preMutation();

                // process the structure changes
                String currentParentKey = null;
                ArrayList<UpdatableModelChild> childrenList = null;
                for (StreamStructure structure : structureChanges) {
                    if (structure.getOperation() == Operation.UPDATE_OR_APPEND) {
                        UpdatableModelChild modelChild =
                                appendedChildren.get(structure.getContentId());
                        if (modelChild == null) {
                            Logger.w(TAG, "Didn't find update child for %s",
                                    structure.getContentId());
                            continue;
                        }
                        if (!modelChild.hasParentId()) {
                            if (!createRoot(modelChild)) {
                                Logger.e(TAG, "Root update failed, invalidating model");
                                Logger.i(TAG, "Moving %s to INVALIDATED",
                                        mSessionId != null ? mSessionId : "No sessionId");

                                UiContext uiContext = mutationContext == null
                                        ? UiContext.getDefaultInstance()
                                        : mutationContext.getUiContext();
                                mCurrentState = ModelState.invalidated(uiContext);
                                mMainThreadRunner.execute(TAG + " multipleRootInvalidation", () -> {
                                    List<ModelProviderObserver> observerList =
                                            getObserversToNotify();
                                    for (ModelProviderObserver observer : observerList) {
                                        observer.onSessionFinished(uiContext);
                                    }
                                });
                                return;
                            }
                            mContents.put(modelChild.getContentId(), modelChild);
                            appends++;
                            continue;
                        }

                        String parentKey = Validators.checkNotNull(modelChild.getParentId());
                        if (!parentKey.equals(currentParentKey)) {
                            childrenList = getChildList(parentKey);
                            currentParentKey = parentKey;
                        }
                        if (childrenList == null) {
                            Logger.e(TAG, "childrenList was not set");
                            continue;
                        }
                        childrenList.add(modelChild);
                        mContents.put(modelChild.getContentId(), modelChild);
                        appends++;

                        mutationHandler.appendChild(parentKey, modelChild);
                    } else if (structure.getOperation() == Operation.REMOVE) {
                        handleRemoveOperation(mutationHandler, structure, removeTracking);
                        removes++;
                        mContents.remove(structure.getContentId());
                    }
                }
            }
            timeTracker.stop("", "modelMutation", "appends", appends, "removes", removes);
        }
    };

    private boolean bindChildrenAndTokens(List<UpdatableModelChild> childrenToBind) {
        // Bind the unbound children
        boolean success = mModelChildBinder.bindChildren(childrenToBind);

        synchronized (mLock) {
            // Track any tokens we added to the tree
            for (UpdatableModelChild child : childrenToBind) {
                if (child.getType() == Type.TOKEN) {
                    String parent = child.getParentId();
                    if (parent == null) {
                        Logger.w(TAG, "Found a token for a child %s without a parent, ignoring",
                                child.getContentId());
                        continue;
                    }
                    ArrayList<UpdatableModelChild> childrenList = getChildList(parent);
                    TokenTracking tokenTracking =
                            new TokenTracking(child.getUpdatableModelToken(), parent, childrenList);
                    mTokens.put(child.getModelToken().getStreamToken().getNextPageToken(),
                            tokenTracking);
                }
            }
        }

        return success;
    }

    private boolean shouldInsertSyntheticToken() {
        synchronized (mLock) {
            return (mRoot != null && mInitialPageSize > 0);
        }
    }

    /** Class which handles Synthetic tokens within the root children list. */
    @VisibleForTesting
    final class SyntheticTokenTracker {
        private final List<UpdatableModelChild> mChildrenToBind = new ArrayList<>();
        private final UpdatableModelChild mPagingChild;
        private final int mStartingPosition;
        private final int mEndPosition;
        private final boolean mInsertToken;

        private UpdatableModelChild mTokenChild;

        SyntheticTokenTracker(UpdatableModelChild pagingChild, int startingPosition, int pageSize) {
            this.mPagingChild = pagingChild;

            if (startingPosition < 0) {
                startingPosition = 0;
            }
            List<UpdatableModelChild> children = mContainers.get(pagingChild.getContentId());
            if (children == null) {
                Logger.e(TAG, "Paging child doesn't not have children");
                this.mStartingPosition = 0;
                this.mEndPosition = 0;
                this.mInsertToken = false;
                return;
            }
            int start = startingPosition;
            int end = startingPosition + pageSize;
            if (children.size() <= start) {
                Logger.e(TAG,
                        "SyntheticTokenTrack to start track beyond child count, start %s,"
                                + " child length %s",
                        startingPosition, children.size());
                // Bind everything
                start = 0;
                end = children.size();
            } else if (start + pageSize > children.size()
                    || start + pageSize + mMinPageSize > children.size()) {
                end = children.size();
            }
            this.mStartingPosition = start;
            this.mEndPosition = end;
            this.mInsertToken = end < children.size();
            Logger.i(TAG, "SyntheticTokenTracker: %d, %d, %d, %b", this.mStartingPosition,
                    this.mEndPosition, children.size(), this.mInsertToken);
        }

        /**
         * Returns the UpdatableModelChild which represents the synthetic token added to the model.
         */
        UpdatableModelChild getTokenChild() {
            return mTokenChild;
        }

        /** Insert a synthetic token into the tree. */
        List<UpdatableModelChild> insertToken() {
            ElapsedTimeTracker tt = mTimingUtils.getElapsedTimeTracker(TAG);
            traverse(mPagingChild, mStartingPosition, mEndPosition);
            if (mInsertToken) {
                synchronized (mLock) {
                    ArrayList<UpdatableModelChild> rootChildren =
                            mContainers.get(mPagingChild.getContentId());
                    if (rootChildren != null) {
                        mTokenChild = getSyntheticToken();
                        rootChildren.add(mEndPosition, mTokenChild);
                        mSyntheticTokens.put(mTokenChild.getContentId(), this);
                        Logger.i(TAG, "Inserting a Synthetic Token %s at %s",
                                mTokenChild.getContentId(), mEndPosition);
                    } else {
                        Logger.e(TAG, "Unable to find paging node's children");
                    }
                }
            }
            tt.stop("", "syntheticTokens");
            return mChildrenToBind;
        }

        /** Handle the synthetic token */
        void handleSyntheticToken(UpdatableModelToken token) {
            StreamToken streamToken = token.getStreamToken();
            SyntheticTokenTracker tracker = null;
            UpdatableModelChild tokenChild = null;
            UpdatableModelChild currentRoot = null;
            List<UpdatableModelChild> rootChildren = null;
            int pos = -1;
            synchronized (mLock) {
                tracker = mSyntheticTokens.get(streamToken.getContentId());
                if (tracker != null) {
                    tokenChild = tracker.getTokenChild();
                }
                currentRoot = mRoot;
                if (tokenChild != null && currentRoot != null) {
                    rootChildren = mContainers.get(currentRoot.getContentId());
                }
                if (rootChildren != null) {
                    pos = rootChildren.indexOf(tokenChild);
                }
            }

            // Raise an error if the token cannot be processed.
            if (pos < 0 || rootChildren == null || currentRoot == null) {
                Logger.e(TAG, "Cannot find synthetic token, %b %b %b %b", tracker != null,
                        tokenChild != null, currentRoot != null, rootChildren != null);
                raiseErrorOnToken(new ModelError(ErrorType.SYNTHETIC_TOKEN_ERROR,
                                          /* continuationToken= */ null),
                        token);
                return;
            }

            // Process the synthetic token.
            synchronized (mLock) {
                mSyntheticTokens.remove(streamToken.getContentId());
            }
            rootChildren.remove(pos);
            SyntheticTokenTracker tokenTracker =
                    new SyntheticTokenTracker(currentRoot, pos, mPageSize);
            List<UpdatableModelChild> childrenToBind = tokenTracker.insertToken();
            List<UpdatableModelChild> cursorSublist =
                    rootChildren.subList(pos, rootChildren.size());

            boolean success = bindChildrenAndTokens(childrenToBind);
            if (!success) {
                Logger.e(TAG, "bindChildren was unsuccessful");
            }

            ModelCursorImpl cursor = new ModelCursorImpl(streamToken.getParentId(), cursorSublist);
            TokenCompleted tokenCompleted = new TokenCompleted(cursor);
            mMainThreadRunner.execute(TAG + " onTokenChange", () -> {
                List<TokenCompletedObserver> observerList = token.getObserversToNotify();
                for (TokenCompletedObserver observer : observerList) {
                    observer.onTokenCompleted(tokenCompleted);
                }
            });
        }

        private void traverse(UpdatableModelChild node, int start, int end) {
            synchronized (mLock) {
                if (node.getType() == Type.UNBOUND) {
                    mChildrenToBind.add(node);
                }
                String nodeId = node.getContentId();
                List<UpdatableModelChild> children = mContainers.get(nodeId);
                if (children != null && !children.isEmpty()) {
                    int maxChildren = Math.min(end, children.size());
                    for (int i = start; i < maxChildren; i++) {
                        UpdatableModelChild child = children.get(i);
                        traverse(child, 0, Integer.MAX_VALUE);
                    }
                }
            }
        }

        private UpdatableModelChild getSyntheticToken() {
            synchronized (mLock) {
                UpdatableModelChild r = Validators.checkNotNull(mRoot);
                String contentId = SYNTHETIC_TOKEN_PREFIX + UUID.randomUUID();
                StreamToken streamToken = StreamToken.newBuilder().setContentId(contentId).build();
                UpdatableModelChild modelChild =
                        new UpdatableModelChild(contentId, r.getContentId());
                modelChild.bindToken(new UpdatableModelToken(streamToken, true));
                return modelChild;
            }
        }
    }

    private void handleRemoveOperation(MutationHandler mutationHandler, StreamStructure removeChild,
            /*@Nullable*/ RemoveTracking<?> removeTracking) {
        if (!removeChild.hasParentContentId()) {
            // It shouldn't be legal to remove the root, that is what CLEAR_HEAD is for.
            Logger.e(TAG, "** Unable to remove the root element");
            return;
        }

        if (removeTracking != null) {
            synchronized (mLock) {
                UpdatableModelChild child = mContents.get(removeChild.getContentId());
                if (child != null) {
                    traverseNode(child, removeTracking);
                } else {
                    Logger.w(TAG, "Didn't find child %s to do RemoveTracking",
                            removeChild.getContentId());
                }
            }
        }
        synchronized (mLock) {
            String parentKey = removeChild.getParentContentId();
            List<UpdatableModelChild> childList = mContainers.get(parentKey);
            if (childList == null) {
                if (!removeChild.hasParentContentId()) {
                    Logger.w(TAG, "Remove of root is not yet supported");
                } else {
                    Logger.w(TAG, "Parent of removed item is not found");
                }
                return;
            }

            // For FEATURE children, add the remove to the mutation handler to create the
            // StreamFeatureChange.  We skip this for TOKENS.
            String childKey = removeChild.getContentId();
            UpdatableModelChild targetChild = mContents.get(childKey);
            if (targetChild == null) {
                if (childKey.startsWith(SYNTHETIC_TOKEN_PREFIX)) {
                    Logger.i(TAG, "Remove Synthetic Token");
                    SyntheticTokenTracker token = mSyntheticTokens.get(childKey);
                    if (token != null) {
                        targetChild = token.getTokenChild();
                        mutationHandler.removeChild(parentKey, targetChild);
                        mSyntheticTokens.remove(childKey);
                    } else {
                        Logger.e(TAG, "Unable to find synthetic token %s", childKey);
                        return;
                    }
                } else {
                    Logger.e(TAG, "Child %s not found in the ModelProvider contents", childKey);
                    return;
                }
            }
            if (targetChild.getType() == Type.FEATURE) {
                mutationHandler.removeChild(parentKey, targetChild);
            } else if (targetChild.getType() == Type.TOKEN) {
                mutationHandler.removeChild(parentKey, targetChild);
            }

            // This walks the child list backwards because the most common removal item is a
            // token which is always the last item in the list.  removeScanCount tracks if we are
            // walking the list too much
            ListIterator<UpdatableModelChild> li = childList.listIterator(childList.size());
            UpdatableModelChild removed = null;
            while (li.hasPrevious()) {
                mRemoveScanCount++;
                UpdatableModelChild child = li.previous();
                if (child.getContentId().equals(childKey)) {
                    removed = child;
                    break;
                }
            }

            if (removed != null) {
                childList.remove(removed);
                mRemovedChildrenCount++;
            } else {
                Logger.w(TAG, "Child to be removed was not found");
            }
        }
    }

    /**
     * This {@link MutationHandler} handles the initial mutation populating the model. No update
     * events are triggered. When the model is updated, we trigger a Session Started event.
     */
    @VisibleForTesting
    final class InitializeModel extends MutationHandler {
        private final UiContext mUiContext;

        InitializeModel(UiContext uiContext) {
            this.mUiContext = uiContext;
        }

        @Override
        public void postMutation() {
            Logger.i(TAG, "Moving %s to READY", mSessionId != null ? mSessionId : "No sessionId");
            synchronized (mLock) {
                mCurrentState = ModelState.ready(mUiContext);
            }
            mMainThreadRunner.execute(TAG + " onSessionStart", () -> {
                List<ModelProviderObserver> observerList = getObserversToNotify();
                for (ModelProviderObserver observer : observerList) {
                    observer.onSessionStart(mUiContext);
                }
            });
        }
    }

    /**
     * This {@link MutationHandler} handles a mutation based upon a continuation token. For a token
     * we will not generate changes for the parent updated by the token. Instead, the new children
     * are appended and a {@link TokenCompleted} will be triggered.
     */
    @VisibleForTesting
    final class TokenMutation extends MutationHandler {
        private final StreamToken mMutationSourceToken;
        /*@Nullable*/ TokenTracking mToken;
        int mNewCursorStart = -1;

        TokenMutation(StreamToken mutationSourceToken) {
            this.mMutationSourceToken = mutationSourceToken;
        }

        @VisibleForTesting
        TokenTracking getTokenTrackingForTest() {
            synchronized (mLock) {
                return Validators.checkNotNull(
                        mTokens.get(mMutationSourceToken.getNextPageToken()));
            }
        }

        @Override
        public void preMutation() {
            synchronized (mLock) {
                mToken = mTokens.remove(mMutationSourceToken.getNextPageToken());
                if (mToken == null) {
                    Logger.e(TAG, "Token was not found, positioning to end of list");
                    return;
                }
                // adjust the location because we will remove the token
                mNewCursorStart = mToken.mLocation.size() - 1;
            }
        }

        @Override
        public void postMutation() {
            if (mToken == null) {
                Logger.e(TAG, "Token was not found, mutation is being ignored");
                return;
            }
            ModelCursorImpl cursor = new ModelCursorImpl(mToken.mParentContentId,
                    mToken.mLocation.subList(mNewCursorStart, mToken.mLocation.size()));
            TokenCompleted tokenCompleted = new TokenCompleted(cursor);
            mMainThreadRunner.execute(TAG + " onTokenChange", () -> {
                if (mToken != null) {
                    List<TokenCompletedObserver> observerList =
                            mToken.mTokenChild.getObserversToNotify();
                    for (TokenCompletedObserver observer : observerList) {
                        observer.onTokenCompleted(tokenCompleted);
                    }
                }
            });
        }
    }

    /**
     * {@code MutationHandler} which handles updates. All changes are tracked for the UI through
     * {@link FeatureChange}. One will be created for each {@link ModelFeature} that changed. There
     * are two types of changes, the content and changes to the children (structure).
     */
    @VisibleForTesting
    final class UpdateMutation extends MutationHandler {
        private final List<StreamStructure> mUpdates;
        private final Map<String, FeatureChangeImpl> mChanges = new HashMap<>();
        private final Set<String> mNewParents = new HashSet<>();

        UpdateMutation(List<StreamStructure> updates) {
            this.mUpdates = updates;
        }

        @Override
        public void preMutation() {
            Logger.i(TAG, "Updating %s items", mUpdates.size());
            // Walk all the updates and update the values, creating changes to track these
            for (StreamStructure update : mUpdates) {
                FeatureChangeImpl change = getChange(update.getContentId());
                if (change != null) {
                    change.setFeatureChanged(true);
                }
            }
        }

        @Override
        public void removeChild(String parentKey, UpdatableModelChild child) {
            FeatureChangeImpl change = getChange(parentKey);
            if (change != null) {
                change.getChildChangesImpl().removeChild(child);
            }
        }

        @Override
        public void appendChild(String parentKey, UpdatableModelChild child) {
            // Is this a child of a node that is new to the model?  We only report changes
            // to existing ModelFeatures.
            String childKey = child.getContentId();
            if (mNewParents.contains(parentKey)) {
                // Don't create a change the child of a new child
                mNewParents.add(childKey);
                return;
            }

            // TODO: this logic assumes that parents are passed before children.
            mNewParents.add(childKey);
            FeatureChangeImpl change = getChange(parentKey);
            if (change != null) {
                change.getChildChangesImpl().addAppendChild(child);
            }
        }

        @Override
        public void postMutation() {
            synchronized (mLock) {
                // Update the cursors before we notify the UI
                List<WeakReference<ModelCursorImpl>> removeList = new ArrayList<>();
                for (WeakReference<ModelCursorImpl> cursorRef : mCursors) {
                    ModelCursorImpl cursor = cursorRef.get();
                    if (cursor != null) {
                        FeatureChange change = mChanges.get(cursor.getParentContentId());
                        if (change != null) {
                            cursor.updateIterator(change);
                        }
                    } else {
                        removeList.add(cursorRef);
                    }
                }
                mCursorsRemoved += removeList.size();
                mCursors.removeAll(removeList);
            }

            // Update the Observers on the UI Thread
            mMainThreadRunner.execute(TAG + " onFeatureChange", () -> {
                for (FeatureChangeImpl change : mChanges.values()) {
                    List<FeatureChangeObserver> observerList =
                            ((UpdatableModelFeature) change.getModelFeature())
                                    .getObserversToNotify();
                    for (FeatureChangeObserver observer : observerList) {
                        observer.onChange(change);
                    }
                }
            });
        }

        /*@Nullable*/
        private FeatureChangeImpl getChange(String contentIdKey) {
            FeatureChangeImpl change = mChanges.get(contentIdKey);
            if (change == null) {
                UpdatableModelChild modelChild;
                synchronized (mLock) {
                    modelChild = mContents.get(contentIdKey);
                }
                if (modelChild == null) {
                    Logger.e(TAG, "Didn't find '%s' in content", contentIdKey);
                    return null;
                }
                if (modelChild.getType() == Type.UNBOUND) {
                    Logger.e(TAG, "Looking for unbound child %s, ignore child",
                            modelChild.getContentId());
                    return null;
                }
                change = new FeatureChangeImpl(modelChild.getModelFeature());
                mChanges.put(contentIdKey, change);
            }
            return change;
        }
    }

    // This method will return true if it sets/updates root
    private boolean createRoot(UpdatableModelChild child) {
        synchronized (mLock) {
            // this must be a root
            if (child.getType() == Type.FEATURE || child.getType() == Type.UNBOUND) {
                if (mRoot != null) {
                    // For multiple roots, check to see if they have the same content id, if so then
                    // ignore the new root.  Otherwise, invalidate the model because we don't
                    // support multiple roots
                    if (mRoot.getContentId().equals(child.getContentId())) {
                        Logger.w(TAG, "Multiple Roots - duplicate root is ignored");
                        return true;
                    } else {
                        Logger.e(TAG,
                                "Found multiple roots [%s, %s] which is not supported."
                                        + "  Invalidating model",
                                Validators.checkNotNull(mRoot).getContentId(),
                                child.getContentId());
                        return false;
                    }
                }
                mRoot = child;
            } else {
                // continuation tokens can not be roots.
                Logger.e(TAG, "Invalid Root, type %s", child.getType());
                return false;
            }
            return true;
        }
    }

    // Lazy creation of containers
    private ArrayList<UpdatableModelChild> getChildList(String parentKey) {
        synchronized (mLock) {
            if (!mContainers.containsKey(parentKey)) {
                mContainers.put(parentKey, new ArrayList<>());
            }
            return mContainers.get(parentKey);
        }
    }

    private void traverseNode(UpdatableModelChild node, RemoveTracking<?> removeTracking) {
        if (node.getType() == Type.FEATURE) {
            removeTracking.filterStreamFeature(node.getModelFeature().getStreamFeature());
            synchronized (mLock) {
                List<UpdatableModelChild> children = mContainers.get(node.getContentId());
                if (children != null) {
                    for (UpdatableModelChild child : children) {
                        traverseNode(child, removeTracking);
                    }
                }
            }
        }
    }

    /** Track the continuation token location and model */
    @VisibleForTesting
    static final class TokenTracking {
        final UpdatableModelToken mTokenChild;
        final String mParentContentId;
        final ArrayList<UpdatableModelChild> mLocation;

        TokenTracking(UpdatableModelToken tokenChild, String parentContentId,
                ArrayList<UpdatableModelChild> location) {
            this.mTokenChild = tokenChild;
            this.mParentContentId = parentContentId;
            this.mLocation = location;
        }
    }

    // test only method for returning a copy of the tokens map
    @VisibleForTesting
    Map<ByteString, TokenTracking> getTokensForTest() {
        synchronized (mLock) {
            return new HashMap<>(mTokens);
        }
    }

    @VisibleForTesting
    boolean getDelayedTriggerRefreshForTest() {
        synchronized (mLock) {
            return mDelayedTriggerRefresh;
        }
    }

    @VisibleForTesting
    @RequestReason
    int getRequestReasonForTest() {
        synchronized (mLock) {
            return mRequestReason;
        }
    }

    @VisibleForTesting
    void clearRootChildrenForTest() {
        synchronized (mLock) {
            if (mRoot == null) {
                return;
            }

            mContainers.remove(mRoot.getContentId());
        }
    }

    private static final class ModelState {
        final UiContext mUiContext;
        @State
        final int mState;

        private ModelState(UiContext uiContext, @State int state) {
            this.mUiContext = uiContext;
            this.mState = state;
        }

        static ModelState initializing() {
            return new ModelState(UiContext.getDefaultInstance(), State.INITIALIZING);
        }

        static ModelState ready(UiContext uiContext) {
            return new ModelState(uiContext, State.READY);
        }

        static ModelState invalidated(UiContext uiContext) {
            return new ModelState(uiContext, State.INVALIDATED);
        }

        public boolean isReady() {
            return mState == State.READY;
        }

        public boolean isInvalidated() {
            return mState == State.INVALIDATED;
        }

        public boolean isInitializing() {
            return mState == State.INITIALIZING;
        }

        @State
        public int getState() {
            return mState;
        }
    }
}
