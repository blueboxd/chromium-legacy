// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent.Listener;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.SessionState;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.api.internal.store.StoreListener;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.Validators;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Function;
import com.google.android.libraries.feed.common.intern.HashPoolInterner;
import com.google.android.libraries.feed.common.intern.InternedMap;
import com.google.android.libraries.feed.common.intern.Interner;
import com.google.android.libraries.feed.common.intern.InternerWithStats;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.logging.StringFormattingUtils;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener;
import com.google.android.libraries.feed.feedsessionmanager.internal.ContentCache;
import com.google.android.libraries.feed.feedsessionmanager.internal.HeadAsStructure;
import com.google.android.libraries.feed.feedsessionmanager.internal.HeadAsStructure.TreeNode;
import com.google.android.libraries.feed.feedsessionmanager.internal.InitializableSession;
import com.google.android.libraries.feed.feedsessionmanager.internal.Session;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionCache;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionFactory;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionManagerMutation;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.PietSharedStateItemProto.PietSharedStateItem;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.annotation.concurrent.GuardedBy;

/** Implementation of the FeedSessionManager. All state is kept in memory. */
public final class FeedSessionManagerImpl
        implements FeedSessionManager, StoreListener, FeedLifecycleListener, Dumpable {
    private static final String TAG = "FeedSessionManagerImpl";

    private static final long TIMEOUT_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(5L);

    // For the Shared State we will always cache them in the Session Manager
    // Accessed on main thread, updated on Executor task
    private final InternerWithStats<StreamSharedState> sharedStateInterner =
            new InternerWithStats<>(new StreamSharedStateInterner());
    private final Map<String, StreamSharedState> sharedStateCache =
            new InternedMap<>(new HashMap<>(), sharedStateInterner);

    // All writes are done on background threads, there are accesses on the main thread.  Leaving
    // the lock back accessibleContentSupplier may eventually run on a background task and not
    // on the executor thread.
    private final SessionCache sessionCache;

    // All access to the content cache happens on the executor thread so there is no need to
    // synchronize access.
    private final ContentCache contentCache;

    @VisibleForTesting
    final AtomicBoolean initialized = new AtomicBoolean(false);

    private final Object lock = new Object();

    // Keep track of sessions being created which haven't been added to the SessionCache.
    // This is accessed on the main thread and the background thread.
    @GuardedBy("lock")
    private final List<InitializableSession> sessionsUnderConstruction = new ArrayList<>();

    // This captures the NO_CARDS_ERROR when a request fails. The request fails in one task and this
    // is sent to the ModelProvider in the populateSessionTask.
    /*@Nullable*/ private ModelError noCardsError;

    private final SessionFactory sessionFactory;
    private final SessionManagerMutation sessionManagerMutation;
    private final Store store;
    private final ThreadUtils threadUtils;
    private final TimingUtils timingUtils;
    private final ProtocolAdapter protocolAdapter;
    private final FeedRequestManager requestManager;
    private final ActionUploadRequestManager actionUploadRequestManager;
    private final SchedulerApi schedulerApi;
    private final TaskQueue taskQueue;
    private final Clock clock;
    private final Configuration configuration;
    private final MainThreadRunner mainThreadRunner;
    private final BasicLoggingApi basicLoggingApi;
    private final long sessionPopulationTimeoutMs;
    private final boolean uploadingActionsEnabled;

    @VisibleForTesting
    final Set<SessionMutationTracker> outstandingMutations = new HashSet<>();

    // operation counts for the dumper
    private int newSessionCount;
    private int existingSessionCount;
    private int handleTokenCount;
    private Listener knownContentListener;

    @SuppressWarnings("argument.type.incompatible") // ok call to registerObserver
    public FeedSessionManagerImpl(TaskQueue taskQueue, SessionFactory sessionFactory,
            SessionCache sessionCache, SessionManagerMutation sessionManagerMutation,
            ContentCache contentCache, Store store, TimingUtils timingUtils,
            ThreadUtils threadUtils, ProtocolAdapter protocolAdapter,
            FeedRequestManager feedRequestManager,
            ActionUploadRequestManager actionUploadRequestManager, SchedulerApi schedulerApi,
            Configuration configuration, Clock clock,
            FeedObservable<FeedLifecycleListener> lifecycleListenerObservable,
            MainThreadRunner mainThreadRunner, BasicLoggingApi basicLoggingApi) {
        this.taskQueue = taskQueue;
        this.sessionFactory = sessionFactory;
        this.sessionCache = sessionCache;
        this.sessionManagerMutation = sessionManagerMutation;
        this.contentCache = contentCache;

        this.store = store;
        this.timingUtils = timingUtils;
        this.threadUtils = threadUtils;
        this.protocolAdapter = protocolAdapter;
        this.requestManager = feedRequestManager;
        this.actionUploadRequestManager = actionUploadRequestManager;
        this.schedulerApi = schedulerApi;
        this.clock = clock;
        this.configuration = configuration;
        this.mainThreadRunner = mainThreadRunner;
        this.basicLoggingApi = basicLoggingApi;
        uploadingActionsEnabled =
                configuration.getValueOrDefault(ConfigKey.UNDOABLE_ACTIONS_ENABLED, false);
        sessionPopulationTimeoutMs =
                configuration.getValueOrDefault(ConfigKey.TIMEOUT_TIMEOUT_MS, TIMEOUT_TIMEOUT_MS);
        lifecycleListenerObservable.registerObserver(this);
        Logger.i(TAG, "FeedSessionManagerImpl has been created");
    }

    /**
     * Called to initialize the session manager. This creates an executor task which does the actual
     * work of setting up the current state. If this is not called, the session manager will not
     * populate new or existing sessions. There isn't error checking on this since this happens on
     * an executor task.
     */
    public void initialize() {
        boolean init = initialized.getAndSet(true);
        if (init) {
            Logger.w(TAG, "FeedSessionManagerImpl has previously been initialized");
            return;
        }
        store.registerObserver(this);
        taskQueue.initialize(this::initializationTask);
    }

    // Task which initializes the Session Manager.  This must be the first task run on the
    // Session Manager thread so it's complete before we create any sessions.
    private void initializationTask() {
        threadUtils.checkNotMainThread();
        Thread currentThread = Thread.currentThread();
        currentThread.setName("JardinExecutor");
        timingUtils.pinThread(currentThread, "JardinExecutor");

        ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
        // Initialize the Shared States cached here.
        ElapsedTimeTracker sharedStateTimeTracker = timingUtils.getElapsedTimeTracker(TAG);
        Result<List<StreamSharedState>> sharedStatesResult = store.getSharedStates();
        if (sharedStatesResult.isSuccessful()) {
            for (StreamSharedState sharedState : sharedStatesResult.getValue()) {
                sharedStateCache.put(sharedState.getContentId(), sharedState);
            }
        } else {
            // without shared states we need to switch to ephemeral mode
            switchToEphemeralMode("SharedStates failed to load, no shared states are loaded.");
            taskQueue.reset();
            sharedStateTimeTracker.stop("", "sharedStateTimeTracker", "error", "store error");
            timeTracker.stop("task", "Initialization", "error", "switchToEphemeralMode");
            return;
        }
        sharedStateTimeTracker.stop("", "sharedStateTimeTracker");

        // create the head session from the data in the Store
        if (!sessionCache.initialize()) {
            // we failed to initialize the sessionCache, so switch to ephemeral mode.
            switchToEphemeralMode("unable to initialize the sessionCache");
            timeTracker.stop("task", "Initialization", "error", "switchToEphemeralMode");
            return;
        }
        timeTracker.stop("task", "Initialization");
    }

    @Override
    public void getNewSession(ModelProvider modelProvider,
            /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext) {
        threadUtils.checkMainThread();
        if (!initialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getNewSession");
            initialize();
        }
        InitializableSession session = sessionFactory.getSession();
        session.bindModelProvider(modelProvider, viewDepthProvider);
        synchronized (lock) {
            sessionsUnderConstruction.add(session);
        }

        if (!sessionCache.isHeadInitialized()) {
            Logger.i(TAG, "Delaying populateSession until initialization is finished");
            taskQueue.execute(Task.GET_NEW_SESSION, TaskType.IMMEDIATE,
                    () -> populateSession(session, uiContext));
        } else {
            populateSession(session, uiContext);
        }
    }

    // This method can be run either on the main thread or on the background thread. It calls the
    // SchedulerApi to determine how the session is created. It creates a new task to populate the
    // new session.
    private void populateSession(InitializableSession session, UiContext uiContext) {
        // Create SessionState and call SchedulerApi to determine what the session-creation
        // experience should be.
        SessionState sessionState = new SessionState(!sessionCache.getHead().isHeadEmpty(),
                sessionCache.getHeadLastAddedTimeMillis(), taskQueue.isMakingRequest());
        Logger.i(TAG,
                "shouldSessionRequestData; hasContent(%b), contentCreationTime(%d),"
                        + " outstandingRequest(%b)",
                sessionState.hasContent, sessionState.contentCreationDateTimeMs,
                sessionState.hasOutstandingRequest);
        @RequestBehavior
        int behavior = schedulerApi.shouldSessionRequestData(sessionState);

        // Based on sessionState and behavior, determine if FeedSessionManager should start a
        // request, append an ongoing request to this session, or include a timeout.
        boolean shouldAppendOutstandingRequest = shouldAppendToSession(sessionState, behavior);
        boolean shouldStartRequest = shouldStartRequest(sessionState, behavior);
        Runnable timeoutTask = shouldPopulateWithTimeout(sessionState, behavior) ? ()
                -> populateSessionTask(session, shouldAppendOutstandingRequest, uiContext)
                : null;
        Logger.i(TAG,
                "shouldSessionRequestDataResult: %s, shouldMakeRequest(%b), withTimeout(%b),"
                        + " withAppend(%b)",
                requestBehaviorToString(behavior), shouldStartRequest, timeoutTask != null,
                shouldAppendOutstandingRequest);

        // If we are making a request, there are two orders, request -> populate for all cases
        // except for REQUEST_WITH_CONTENT which is populate -> request.
        if (shouldStartRequest && behavior != RequestBehavior.REQUEST_WITH_CONTENT) {
            triggerRefresh(/* sessionId= */ null, RequestReason.OPEN_WITHOUT_CONTENT, uiContext);
        }
        taskQueue.execute(Task.POPULATE_NEW_SESSION, requestBehaviorToTaskType(behavior),
                ()
                        -> populateSessionTask(session, shouldAppendOutstandingRequest, uiContext),
                timeoutTask, sessionPopulationTimeoutMs);
        if (shouldStartRequest && behavior == RequestBehavior.REQUEST_WITH_CONTENT) {
            triggerRefresh(/* sessionId= */ null, RequestReason.OPEN_WITH_CONTENT, uiContext);
        }
    }

    private void populateSessionTask(InitializableSession session,
            boolean shouldAppendOutstandingRequest, UiContext uiContext) {
        threadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);

        if (noCardsError != null && sessionCache.getHead().isHeadEmpty()) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider == null) {
                Logger.e(TAG, "populateSessionTask - noCardsError, modelProvider not found");
                timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
                return;
            }
            Logger.w(TAG, "populateSessionTask - noCardsError %s", modelProvider);

            Result<String> streamSessionResult = store.createNewSession();
            if (!streamSessionResult.isSuccessful()) {
                switchToEphemeralMode("Unable to create a new session during noCardsError failure");
                timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
                return;
            }

            // properly track the session so that it's empty.
            modelProvider.raiseError(Validators.checkNotNull(noCardsError));
            String sessionId = streamSessionResult.getValue();
            session.setSessionId(sessionId);
            sessionCache.putAttached(sessionId, clock.currentTimeMillis(),
                    sessionCache.getHead().getSchemaVersion(), session);
            synchronized (lock) {
                sessionsUnderConstruction.remove(session);
            }

            // Set the session id on the ModelProvider.
            modelProvider.edit().setSessionId(sessionId).commit();
            timeTracker.stop("task", "Create/Populate New Session", "Failure", "noCardsError");
            return;
        }

        Result<String> streamSessionResult = store.createNewSession();
        if (!streamSessionResult.isSuccessful()) {
            switchToEphemeralMode("Unable to create a new session, createNewSession failed");
            timeTracker.stop("task", "Create/Populate New Session", "Failure", "createNewSession");
            return;
        }
        String sessionId = streamSessionResult.getValue();
        session.setSessionId(sessionId);
        Result<List<StreamStructure>> streamStructuresResult = store.getStreamStructures(sessionId);
        if (!streamStructuresResult.isSuccessful()) {
            switchToEphemeralMode("Unable to create a new session, getStreamStructures failed");
            timeTracker.stop(
                    "task", "Create/Populate New Session", "Failure", "getStreamStructures");
            return;
        }

        boolean cachedBindings;
        cachedBindings = contentCache.size() > 0;
        long creationTimeMillis = clock.currentTimeMillis();
        session.populateModelProvider(streamStructuresResult.getValue(), cachedBindings,
                shouldAppendOutstandingRequest, uiContext);
        sessionCache.putAttached(
                sessionId, creationTimeMillis, sessionCache.getHead().getSchemaVersion(), session);
        synchronized (lock) {
            sessionsUnderConstruction.remove(session);
        }
        newSessionCount++;
        Logger.i(TAG, "Populate new session: %s, creation time %s", session.getSessionId(),
                StringFormattingUtils.formatLogDate(creationTimeMillis));
        timeTracker.stop("task", "Create/Populate New Session");
    }

    @VisibleForTesting
    void switchToEphemeralMode(String message) {
        Logger.e(TAG, message);
        store.switchToEphemeralMode();
    }

    @VisibleForTesting
    void modelErrorObserver(/*@Nullable*/ Session session, ModelError error) {
        if (session == null && error.getErrorType() == ErrorType.NO_CARDS_ERROR) {
            Logger.e(TAG, "No Cards Found on TriggerRefresh, setting noCardsError");
            noCardsError = error;
            // queue a clear which will run after all currently delayed tasks.  This allows delayed
            // session population tasks to inform the ModelProvider of errors then we clear the
            // error state.
            taskQueue.execute(
                    Task.NO_CARD_ERROR_CLEAR, TaskType.USER_FACING, () -> noCardsError = null);
            return;
        } else if (session != null && error.getErrorType() == ErrorType.PAGINATION_ERROR) {
            Logger.e(TAG, "Pagination Error found");
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.raiseError(error);
            } else {
                Logger.e(TAG, "handling Pagination Error, didn't find Model Provider");
            }
            return;
        }
        Logger.e(TAG, "unhandled modelErrorObserver: session, %s, error %s", session != null,
                error.getErrorType());
    }

    @Override
    public void getExistingSession(
            String sessionId, ModelProvider modelProvider, UiContext uiContext) {
        threadUtils.checkMainThread();
        if (!initialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getExistingSession");
            initialize();
        }
        InitializableSession session = sessionFactory.getSession();
        session.bindModelProvider(modelProvider, null);

        // Task which populates the newly created session.  This must be done
        // on the Session Manager thread so it atomic with the mutations.
        taskQueue.execute(Task.GET_EXISTING_SESSION, TaskType.IMMEDIATE, () -> {
            threadUtils.checkNotMainThread();
            if (!sessionCache.hasSession(sessionId)) {
                modelProvider.invalidate(uiContext);
                return;
            }
            Session existingSession = sessionCache.getAttached(sessionId);
            if (existingSession != null && !existingSession.getContentInSession().isEmpty()) {
                ModelProvider existingModelProvider = existingSession.getModelProvider();
                if (existingModelProvider != null) {
                    existingModelProvider.invalidate(uiContext);
                }
            }

            Result<List<StreamStructure>> streamStructuresResult =
                    store.getStreamStructures(sessionId);
            if (streamStructuresResult.isSuccessful()) {
                session.setSessionId(sessionId);
                session.populateModelProvider(
                        streamStructuresResult.getValue(), false, false, uiContext);
                sessionCache.putAttachedAndRetainMetadata(sessionId, session);
                existingSessionCount++;
            } else {
                Logger.e(TAG, "unable to get stream structure for existing session %s", sessionId);
                switchToEphemeralMode("unable to get stream structure for existing session");
            }
        });
    }

    @Override
    public void invalidateSession(String sessionId) {
        if (threadUtils.isMainThread()) {
            taskQueue.execute(Task.INVALIDATE_SESSION, TaskType.USER_FACING,
                    () -> sessionCache.removeAttached(sessionId));
        } else {
            sessionCache.removeAttached(sessionId);
        }
    }

    @Override
    public void detachSession(String sessionId) {
        if (threadUtils.isMainThread()) {
            taskQueue.execute(Task.DETACH_SESSION, TaskType.USER_FACING,
                    () -> sessionCache.detachModelProvider(sessionId));
        } else {
            sessionCache.detachModelProvider(sessionId);
        }
    }

    @Override
    public void invalidateHead() {
        sessionManagerMutation.resetHead();
    }

    @Override
    public void handleToken(String sessionId, StreamToken streamToken) {
        Logger.i(TAG, "HandleToken on stream %s, token %s", sessionId, streamToken.getContentId());
        threadUtils.checkMainThread();

        // At the moment, this doesn't try to prevent multiple requests with the same Token.
        // We may want to make sure we only make the request a single time.
        handleTokenCount++;
        MutationContext mutationContext = new MutationContext.Builder()
                                                  .setContinuationToken(streamToken)
                                                  .setRequestingSessionId(sessionId)
                                                  .build();
        taskQueue.execute(Task.HANDLE_TOKEN, TaskType.BACKGROUND, () -> {
            fetchActionsAndUpload(getConsistencyToken(), result -> {
                ConsistencyToken token = handleUpdateConsistencyToken(result);
                requestManager.loadMore(
                        streamToken, token, getCommitter("handleToken", mutationContext));
            });
        });
    }

    @Override
    public void triggerRefresh(/*@Nullable*/ String sessionId) {
        triggerRefresh(sessionId, RequestReason.HOST_REQUESTED, UiContext.getDefaultInstance());
    }

    @Override
    public void triggerRefresh(
            /*@Nullable*/ String sessionId, @RequestReason int requestReason, UiContext uiContext) {
        if (!initialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, triggerRefresh");
            initialize();
        }
        taskQueue.execute(Task.SESSION_MANAGER_TRIGGER_REFRESH,
                TaskType.HEAD_INVALIDATE, // invalidate because we are requesting a refresh
                () -> triggerRefreshTask(sessionId, requestReason, uiContext));
    }

    private ConsistencyToken handleUpdateConsistencyToken(Result<ConsistencyToken> result) {
        threadUtils.checkNotMainThread();
        if (!uploadingActionsEnabled) {
            return getConsistencyToken();
        }

        ConsistencyToken consistencyToken;
        if (result.isSuccessful()) {
            consistencyToken = result.getValue();
            store.editContent()
                    .add(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID,
                            StreamPayload.newBuilder()
                                    .setConsistencyToken(consistencyToken)
                                    .build())
                    .commit();

        } else {
            consistencyToken = getConsistencyToken();
            Logger.w(TAG, "TriggerRefresh didn't get a consistencyToken Back");
        }
        return consistencyToken;
    }

    private void triggerRefreshTask(
            /*@Nullable*/ String sessionId, @RequestReason int requestReason, UiContext uiContext) {
        threadUtils.checkNotMainThread();

        fetchActionsAndUpload(getConsistencyToken(), result -> {
            ConsistencyToken consistencyToken = handleUpdateConsistencyToken(result);
            requestManager.triggerRefresh(requestReason, consistencyToken,
                    getCommitter("triggerRefresh",
                            new MutationContext.Builder().setUiContext(uiContext).build()));
        });

        if (sessionId != null) {
            Session session = sessionCache.getAttached(sessionId);
            if (session != null) {
                ModelProvider modelProvider = session.getModelProvider();
                if (modelProvider != null) {
                    invalidateSessionInternal(modelProvider, session, uiContext);
                } else {
                    Logger.w(TAG, "Session didn't have a ModelProvider %s", sessionId);
                }
            } else {
                Logger.w(TAG, "TriggerRefresh didn't find session %s", sessionId);
            }
        } else {
            Logger.i(TAG, "triggerRefreshTask no StreamSession provided");
        }
    }

    @Override
    public void fetchActionsAndUpload(Consumer<Result<ConsistencyToken>> consumer) {
        threadUtils.checkNotMainThread();
        fetchActionsAndUpload(getConsistencyToken(), consumer);
    }

    private void fetchActionsAndUpload(
            ConsistencyToken token, Consumer<Result<ConsistencyToken>> consumer) {
        // fail fast if we're not actually recording these actions.
        if (!uploadingActionsEnabled) {
            consumer.accept(Result.success(token));
            return;
        }
        Result<Set<StreamUploadableAction>> actionsResult = store.getAllUploadableActions();
        if (actionsResult.isSuccessful()) {
            actionUploadRequestManager.triggerUploadActions(
                    actionsResult.getValue(), token, consumer);
        }
    }

    @Override
    public Result<List<PayloadWithId>> getStreamFeatures(List<String> contentIds) {
        threadUtils.checkNotMainThread();
        List<PayloadWithId> results = new ArrayList<>();
        List<String> cacheMisses = new ArrayList<>();
        int contentSize = contentCache.size();
        for (String contentId : contentIds) {
            StreamPayload payload = contentCache.get(contentId);
            if (payload != null) {
                results.add(new PayloadWithId(contentId, payload));
            } else {
                cacheMisses.add(contentId);
            }
        }

        if (!cacheMisses.isEmpty()) {
            Result<List<PayloadWithId>> contentResult = store.getPayloads(cacheMisses);
            boolean successfulRead = contentResult.isSuccessful()
                    && (contentResult.getValue().size()
                                    + configuration.getValueOrDefault(
                                            ConfigKey.STORAGE_MISS_THRESHOLD, 4L)
                            >= cacheMisses.size());
            if (successfulRead) {
                Logger.i(TAG, "getStreamFeatures; requestedItems(%d), result(%d)",
                        cacheMisses.size(), contentResult.getValue().size());
                if (contentResult.getValue().size() < cacheMisses.size()) {
                    Logger.e(TAG, "ContentStorage is missing content");
                    mainThreadRunner.execute("CONTENT_STORAGE_MISSING_ITEM", () -> {
                        basicLoggingApi.onInternalError(
                                InternalFeedError.CONTENT_STORAGE_MISSING_ITEM);
                    });
                }
                results.addAll(contentResult.getValue());
            } else {
                if (contentResult.isSuccessful()) {
                    Logger.e(TAG, "Storage miss beyond threshold; requestedItems(%d), returned(%d)",
                            cacheMisses.size(), contentResult.getValue().size());
                    mainThreadRunner.execute("STORAGE_MISS_BEYOND_THRESHOLD", () -> {
                        basicLoggingApi.onInternalError(
                                InternalFeedError.STORAGE_MISS_BEYOND_THRESHOLD);
                    });
                }

                // since we couldn't populate the content, switch to ephemeral mode
                switchToEphemeralMode("Unable to get the payloads in getStreamFeatures");
                return Result.failure();
            }
        }
        Logger.i(TAG, "Caching getStreamFeatures - items %s, cache misses %s, cache size %s",
                contentIds.size(), cacheMisses.size(), contentSize);
        return Result.success(results);
    }

    @Override
    /*@Nullable*/
    public StreamSharedState getSharedState(ContentId contentId) {
        threadUtils.checkMainThread();
        String sharedStateId = protocolAdapter.getStreamContentId(contentId);
        StreamSharedState state = sharedStateCache.get(sharedStateId);
        if (state == null) {
            Logger.e(TAG, "Shared State [%s] was not found", sharedStateId);
        }
        return state;
    }

    @Override
    public Consumer<Result<Model>> getUpdateConsumer(MutationContext mutationContext) {
        if (!initialized.get()) {
            Logger.i(TAG, "Lazy initialization triggered, getUpdateConsumer");
            initialize();
        }
        return new SessionMutationTracker(mutationContext, "updateConsumer");
    }

    @VisibleForTesting
    class SessionMutationTracker implements Consumer<Result<Model>> {
        private final MutationContext mutationContext;
        private final String taskName;

        @SuppressWarnings("argument.type.incompatible") // ok to add this to the map
        private SessionMutationTracker(MutationContext mutationContext, String taskName) {
            this.mutationContext = mutationContext;
            this.taskName = taskName;
            outstandingMutations.add(this);
        }

        @Override
        public void accept(Result<Model> input) {
            if (outstandingMutations.remove(this)) {
                if (input.isSuccessful()) {
                    updateSharedStateCache(input.getValue().streamDataOperations);
                }
                sessionManagerMutation
                        .createCommitter(taskName, mutationContext,
                                FeedSessionManagerImpl.this::modelErrorObserver,
                                knownContentListener)
                        .accept(input);
            } else {
                Logger.w(TAG, "SessionMutationTracker dropping response due to clear");
            }
        }
    }

    @Override
    public <T> void getStreamFeaturesFromHead(
            Function<StreamPayload, /*@Nullable*/ T> filterPredicate,
            Consumer<Result<List</*@NonNull*/ T>>> consumer) {
        taskQueue.execute(Task.GET_STREAM_FEATURES_FROM_HEAD, TaskType.BACKGROUND, () -> {
            HeadAsStructure headAsStructure = new HeadAsStructure(store, timingUtils, threadUtils);
            Function<TreeNode, /*@Nullable*/ T> toStreamPayload =
                    treeNode -> filterPredicate.apply(treeNode.getStreamPayload());
            headAsStructure.initialize(result -> {
                if (!result.isSuccessful()) {
                    consumer.accept(Result.failure());
                    return;
                }
                Result<List</*@NonNull*/ T>> filterResults =
                        headAsStructure.filter(toStreamPayload);
                consumer.accept(filterResults.isSuccessful()
                                ? Result.success(filterResults.getValue())
                                : Result.failure());
            });
        });
    }

    @Override
    public void setKnownContentListener(KnownContent.Listener knownContentListener) {
        this.knownContentListener = knownContentListener;
    }

    @Override
    public void onSwitchToEphemeralMode() {
        reset();
    }

    private Consumer<Result<Model>> getCommitter(String taskName, MutationContext mutationContext) {
        return new SessionMutationTracker(mutationContext, taskName);
    }

    @Override
    public void reset() {
        threadUtils.checkNotMainThread();
        sessionManagerMutation.forceResetHead();
        sessionCache.reset();
        // Invalidate all sessions currently under construction
        List<InitializableSession> invalidateSessions;
        synchronized (lock) {
            invalidateSessions = new ArrayList<>(sessionsUnderConstruction);
            sessionsUnderConstruction.clear();
        }
        for (InitializableSession session : invalidateSessions) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.invalidate();
            }
        }
        contentCache.reset();
        sharedStateCache.clear();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("newSessionCount").value(newSessionCount);
        dumper.forKey("existingSessionCount").value(existingSessionCount).compactPrevious();
        dumper.forKey("handleTokenCount").value(handleTokenCount).compactPrevious();
        dumper.forKey("sharedStateSize").value(sharedStateCache.size()).compactPrevious();
        dumper.forKey("sharedStateInternerSize")
                .value(sharedStateInterner.size())
                .compactPrevious();
        dumper.forKey("sharedStateInternerStats")
                .value(sharedStateInterner.getStats())
                .compactPrevious();
        dumper.dump(contentCache);
        dumper.dump(taskQueue);
        dumper.dump(sessionCache);
        dumper.dump(sessionManagerMutation);
    }

    private void invalidateSessionInternal(
            ModelProvider modelProvider, Session session, UiContext uiContext) {
        threadUtils.checkNotMainThread();
        Logger.i(TAG, "Invalidate session %s", session.getSessionId());
        modelProvider.invalidate(uiContext);
    }

    // This is only used in tests to verify the contents of the shared state cache.
    @VisibleForTesting
    Map<String, StreamSharedState> getSharedStateCacheForTest() {
        return new HashMap<>(sharedStateCache);
    }

    // This is only used in tests to access a the associated sessions.
    @VisibleForTesting
    SessionCache getSessionCacheForTest() {
        return sessionCache;
    }

    // Called in the integration tests
    @VisibleForTesting
    public boolean isDelayed() {
        return taskQueue.isDelayed();
    }

    @Override
    public void onLifecycleEvent(@LifecycleEvent String event) {
        Logger.i(TAG, "onLifecycleEvent %s", event);
        switch (event) {
            case LifecycleEvent.INITIALIZE:
                initialize();
                break;
            case LifecycleEvent.CLEAR_ALL:
                Logger.i(TAG, "CLEAR_ALL will cancel %s mutations", outstandingMutations.size());
                outstandingMutations.clear();
                break;
            case LifecycleEvent.CLEAR_ALL_WITH_REFRESH:
                Logger.i(TAG, "CLEAR_ALL_WITH_REFRESH will cancel %s mutations",
                        outstandingMutations.size());
                outstandingMutations.clear();
                break;
            default:
                // Do nothing
        }
    }
    // TODO: implement longer term fix for reading/saving the consistency token
    @Override
    public void triggerUploadActions(Set<StreamUploadableAction> actions) {
        threadUtils.checkNotMainThread();

        actionUploadRequestManager.triggerUploadActions(
                actions, getConsistencyToken(), getConsistencyTokenConsumer());
    }

    @VisibleForTesting
    ConsistencyToken getConsistencyToken() {
        threadUtils.checkNotMainThread();

        // don't bother with reading consistencytoken if we're not uploading actions.
        if (!uploadingActionsEnabled) {
            return ConsistencyToken.getDefaultInstance();
        }
        Result<List<PayloadWithId>> contentResult = store.getPayloads(
                Collections.singletonList(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID));
        if (contentResult.isSuccessful()) {
            for (PayloadWithId payload : contentResult.getValue()) {
                if (payload.payload.hasConsistencyToken()) {
                    return payload.payload.getConsistencyToken();
                }
            }
        }
        return ConsistencyToken.getDefaultInstance();
    }

    @VisibleForTesting
    Consumer<Result<ConsistencyToken>> getConsistencyTokenConsumer() {
        return result -> {
            if (result.isSuccessful()) {
                store.editContent()
                        .add(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID,
                                StreamPayload.newBuilder()
                                        .setConsistencyToken(result.getValue())
                                        .build())
                        .commit();
            }
        };
    }

    private void updateSharedStateCache(List<StreamDataOperation> updates) {
        for (StreamDataOperation dataOperation : updates) {
            Operation operation = dataOperation.getStreamStructure().getOperation();
            if ((operation == Operation.UPDATE_OR_APPEND)
                    && SessionManagerMutation.validDataOperation(dataOperation)) {
                String contentId = dataOperation.getStreamStructure().getContentId();
                StreamPayload payload = dataOperation.getStreamPayload();
                if (payload.hasStreamSharedState()) {
                    sharedStateCache.put(contentId, payload.getStreamSharedState());
                }
            }
        }
    }

    private static boolean shouldAppendToSession(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.REQUEST_WITH_TIMEOUT:
                return sessionState.hasContent;
            case RequestBehavior.NO_REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return sessionState.hasContent && sessionState.hasOutstandingRequest;
            default:
                return false;
        }
    }

    private static boolean shouldStartRequest(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        return (requestBehavior == RequestBehavior.REQUEST_WITH_TIMEOUT
                       || requestBehavior == RequestBehavior.REQUEST_WITH_WAIT
                       || requestBehavior == RequestBehavior.REQUEST_WITH_CONTENT)
                && !sessionState.hasOutstandingRequest;
    }

    private static boolean shouldPopulateWithTimeout(
            SessionState sessionState, @RequestBehavior int requestBehavior) {
        return requestBehavior == RequestBehavior.REQUEST_WITH_TIMEOUT
                || (requestBehavior == RequestBehavior.NO_REQUEST_WITH_TIMEOUT
                        && sessionState.hasOutstandingRequest);
    }

    private static String requestBehaviorToString(@RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.NO_REQUEST_WITH_CONTENT:
                return "NO_REQUEST_WITH_CONTENT";
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return "NO_REQUEST_WITH_TIMEOUT";
            case RequestBehavior.NO_REQUEST_WITH_WAIT:
                return "NO_REQUEST_WITH_WAIT";
            case RequestBehavior.REQUEST_WITH_CONTENT:
                return "REQUEST_WITH_CONTENT";
            case RequestBehavior.REQUEST_WITH_TIMEOUT:
                return "REQUEST_WITH_TIMEOUT";
            case RequestBehavior.REQUEST_WITH_WAIT:
                return "REQUEST_WITH_WAIT";
            default:
                return "UNKNOWN";
        }
    }

    private static int requestBehaviorToTaskType(@RequestBehavior int requestBehavior) {
        switch (requestBehavior) {
            case RequestBehavior.REQUEST_WITH_WAIT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_WAIT:
                // Wait for the request to complete and then show content.
                return TaskType.USER_FACING;
            case RequestBehavior.REQUEST_WITH_CONTENT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_CONTENT:
                // Show content immediately and append when the request completes.
                return TaskType.IMMEDIATE;
            case RequestBehavior.REQUEST_WITH_TIMEOUT: // Fall-through
            case RequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                // Wait for the request to complete but show current content if the timeout elapses.
                return TaskType.USER_FACING;
            default:
                return TaskType.USER_FACING;
        }
    }

    // Interner that caches the inner PietSharedStateItem from a StreamSharedState, which may
    // sometimes be identical, only the inner content_id differing (see [INTERNAL LINK]).
    @VisibleForTesting
    static class StreamSharedStateInterner implements Interner<StreamSharedState> {
        private final Interner<PietSharedStateItem> interner = new HashPoolInterner<>();

        @SuppressWarnings("ReferenceEquality")
        // Intentional reference comparison for interned != orig
        @Override
        public StreamSharedState intern(StreamSharedState input) {
            PietSharedStateItem orig = input.getPietSharedStateItem();
            PietSharedStateItem interned = interner.intern(orig);
            if (interned != orig) {
                /*
                 * If interned != orig we have a memoized item and we need to replace the proto with
                 * the modified version.
                 */
                return input.toBuilder().setPietSharedStateItem(interned).build();
            }
            return input;
        }

        @Override
        public void clear() {
            interner.clear();
        }

        @Override
        public int size() {
            return interner.size();
        }
    }
}
