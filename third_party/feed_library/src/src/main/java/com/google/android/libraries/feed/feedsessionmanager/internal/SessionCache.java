// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.Validators;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.logging.StringFormattingUtils;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.search.now.feed.client.StreamDataProto.SessionMetadata;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSession;
import com.google.search.now.feed.client.StreamDataProto.StreamSessions;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.annotation.concurrent.GuardedBy;

/**
 * For the FeedSessionManager, this class is a cache of the {@link Session} objects which are
 * currently defined.
 */
public final class SessionCache implements Dumpable {
    private static final String TAG = "SessionCache";
    @VisibleForTesting
    static final int MIN_SCHEMA_VERSION_FOR_PIET_SHARED_STATE_REQUIRED_CONTENT = 2;
    @VisibleForTesting
    static final String STREAM_SESSION_CONTENT_ID = "FSM::Sessions::0";

    /**
     * Key used to store the consistency token in {@link
     * com.google.android.libraries.feed.api.host.storage.ContentStorage}
     */
    public static final String CONSISTENCY_TOKEN_CONTENT_ID = "ct";

    // Used to synchronize the stored data
    private final Object lock = new Object();

    // Sessions with ModelProviders (attached).
    @GuardedBy("lock")
    private final Map<String, Session> attachedSessions = new HashMap<>();

    @GuardedBy("lock")
    private final Map<String, SessionMetadata> sessionsMetadata = new HashMap<>();

    private final HeadSessionImpl head;

    private final Store store;
    private final TaskQueue taskQueue;
    private final SessionFactory sessionFactory;
    private final long lifetimeMs;
    private final TimingUtils timingUtils;
    private final ThreadUtils threadUtils;
    private final Clock clock;

    private boolean initialized;

    // operation counts for the dumper
    private int getCount;
    private int getAttachedCount;
    private int getAllCount;
    private int putCount;
    private int removeCount;
    private int unboundSessionCount;
    private int detachedSessionCount;
    private int expiredSessionsCleared;

    public SessionCache(Store store, TaskQueue taskQueue, SessionFactory sessionFactory,
            long lifetimeMs, TimingUtils timingUtils, ThreadUtils threadUtil, Clock clock) {
        this.store = store;
        this.taskQueue = taskQueue;
        this.sessionFactory = sessionFactory;
        this.lifetimeMs = lifetimeMs;
        this.timingUtils = timingUtils;
        this.threadUtils = threadUtil;
        this.clock = clock;

        this.head = sessionFactory.getHeadSession();
    }

    /**
     * Return {@link HeadSessionImpl} for head. Returns {@code null} if {@link #initialize()} hasn't
     * finished running.
     */
    public HeadSessionImpl getHead() {
        return head;
    }

    /** Returns {@code true} if HEAD has been initialize, which happens in {@link #initialize()}. */
    public boolean isHeadInitialized() {
        return initialized;
    }

    /**
     * Returns true if sessionId exists in any of {attached, unbound, head} set, false otherwise.
     */
    public boolean hasSession(String sessionId) {
        synchronized (lock) {
            return sessionsMetadata.containsKey(sessionId);
        }
    }

    /** Returns all attached sessions tracked by the SessionCache. This does NOT include head. */
    public List<Session> getAttachedSessions() {
        getAttachedCount++;
        synchronized (lock) {
            Logger.d(TAG, "getAttachedSessions, size=%d", attachedSessions.size());
            return new ArrayList<>(attachedSessions.values());
        }
    }

    /**
     * Returns all sessions tracked by the SessionCache. This includes head and previously thrown
     * away unbounded sessions, which are re-created.
     *
     * <p>NOTE: This method should be called from a background thread, as it reads the persistent
     * storage in order to populate the unbound sessions.
     */
    List<Session> getAllSessions() {
        threadUtils.checkNotMainThread();
        getAllCount++;

        // Re-build the unbound sessions (that were thrown away on detach) and add them.
        List<Session> allSessions = new ArrayList<>(populateUnboundSessions());

        allSessions.add(head);
        synchronized (lock) {
            allSessions.addAll(attachedSessions.values());
        }

        Logger.d(TAG, "getAllSessions, size=%d", allSessions.size());
        return allSessions;
    }

    /**
     * Return an attached {@link Session} for the sessionId, or {@code null} if the sessionCache
     * doesn't contain the Session or it is no longer attached.
     */
    /*@Nullable*/
    public Session getAttached(String sessionId) {
        getCount++;
        synchronized (lock) {
            return attachedSessions.get(sessionId);
        }
    }

    /** Returns the last time content was added to HEAD. */
    public long getHeadLastAddedTimeMillis() {
        synchronized (lock) {
            SessionMetadata metadata = sessionsMetadata.get(head.getSessionId());
            if (metadata == null) {
                Logger.e(TAG, "SessionMetadata missing for HEAD");
                return 0L;
            }
            return metadata.getLastAddedTimeMillis();
        }
    }

    /** Add a {@link Session} to the SessionCache. */
    public void putAttached(
            String sessionId, long creationTimeMillis, int schemaVersion, Session session) {
        Logger.d(TAG, "putAttached, sessionId=%s", sessionId);

        threadUtils.checkNotMainThread();
        putCount++;
        synchronized (lock) {
            attachedSessions.put(sessionId, session);
            sessionsMetadata.put(sessionId,
                    SessionMetadata.newBuilder()
                            .setCreationTimeMillis(creationTimeMillis)
                            .setSchemaVersion(schemaVersion)
                            .build());
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", attachedSessions.size(),
                    sessionsMetadata.size());
        }
        updatePersistedSessionsMetadata();
    }

    /** Add a {@link Session} to the SessionCache and retain the existing metadata. */
    public void putAttachedAndRetainMetadata(String sessionId, Session session) {
        Logger.d(TAG, "putAttachedAndRetainMetadata, sessionId=%s", sessionId);

        threadUtils.checkNotMainThread();
        putCount++;
        synchronized (lock) {
            attachedSessions.put(sessionId, session);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", attachedSessions.size(),
                    sessionsMetadata.size());
        }
    }

    /**
     * Detaches the model provider from the session, which becomes unbound. We throw away unbound
     * sessions for memory reasons, they are re-created on demand when needed in {@link
     * #populateUnboundSessions()}.
     */
    public void detachModelProvider(String sessionId) {
        Logger.d(TAG, "detachModelProvider, sessionId=%s", sessionId);

        threadUtils.checkNotMainThread();
        InitializableSession initializableSession;
        synchronized (lock) {
            Session session = attachedSessions.get(sessionId);
            if (!(session instanceof InitializableSession)) {
                Logger.w(TAG, "Unable to detach session %s", sessionId);
                return;
            } else {
                initializableSession = (InitializableSession) session;
            }
            attachedSessions.remove(sessionId);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", attachedSessions.size(),
                    sessionsMetadata.size());
        }

        initializableSession.bindModelProvider(null, null);
        detachedSessionCount++;
    }

    /** Remove an attached {@link Session} from the SessionCache. */
    public void removeAttached(String sessionId) {
        Logger.d(TAG, "removeAttached, sessionId=%s", sessionId);

        threadUtils.checkNotMainThread();
        removeCount++;
        synchronized (lock) {
            attachedSessions.remove(sessionId);
            sessionsMetadata.remove(sessionId);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", attachedSessions.size(),
                    sessionsMetadata.size());
        }
        updatePersistedSessionsMetadata();
    }

    /** Initialize the SessionCache from Store. */
    public boolean initialize() {
        threadUtils.checkNotMainThread();

        // create the head session from the data in the Store
        ElapsedTimeTracker headTimeTracker = timingUtils.getElapsedTimeTracker(TAG);
        Result<List<StreamStructure>> results = store.getStreamStructures(head.getSessionId());
        if (!results.isSuccessful()) {
            Logger.w(TAG, "unable to get head stream structures");
            return false;
        }

        List<StreamSession> sessionList = getPersistedSessions();
        int headSchemaVersion = getHeadSchemaVersion(sessionList);
        head.initializeSession(results.getValue(), headSchemaVersion);
        initialized = true;
        headTimeTracker.stop("", "createHead");

        initializePersistedSessions(sessionList);

        // Ensure that SessionMetadata exists for HEAD.
        synchronized (lock) {
            if (!sessionsMetadata.containsKey(head.getSessionId())) {
                sessionsMetadata.put(head.getSessionId(), SessionMetadata.getDefaultInstance());
            }
            Logger.d(TAG, "initialize, size=%d", sessionsMetadata.size());
        }
        return true;
    }

    public void reset() {
        List<Session> sessionList;
        synchronized (lock) {
            sessionList = new ArrayList<>(attachedSessions.values());
        }
        for (Session session : sessionList) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.invalidate();
            }
        }
        synchronized (lock) {
            attachedSessions.clear();
            sessionsMetadata.clear();
            head.reset();
            sessionsMetadata.put(head.getSessionId(), SessionMetadata.getDefaultInstance());
            initialized = true;
        }
    }

    /** Returns when the specified {@code sessionId} was created. */
    @VisibleForTesting
    long getCreationTimeMillis(String sessionId) {
        synchronized (lock) {
            SessionMetadata metadata = sessionsMetadata.get(sessionId);
            if (metadata == null) {
                Logger.e(TAG, "SessionMetadata missing for session %s", sessionId);
                return 0L;
            }
            return metadata.getCreationTimeMillis();
        }
    }

    /**
     * Updates the last time content was added to HEAD and its schema version and writes data to
     * disk.
     */
    void updateHeadMetadata(long lastAddedTimeMillis, int schemaVersion) {
        threadUtils.checkNotMainThread();
        synchronized (lock) {
            SessionMetadata metadata = sessionsMetadata.get(head.getSessionId());
            SessionMetadata.Builder builder =
                    metadata == null ? SessionMetadata.newBuilder() : metadata.toBuilder();
            sessionsMetadata.put(head.getSessionId(),
                    builder.setLastAddedTimeMillis(lastAddedTimeMillis)
                            .setSchemaVersion(schemaVersion)
                            .build());
        }
        updatePersistedSessionsMetadata();
    }

    /**
     * Creates a list of unbound sessions, which are populated from persisted storage. Unbound
     * sessions are defined as the set of sessions whose IDs are in the set {sessionsMetadata.keys -
     * sessions.keys - head.key}, i.e sessions that were detached and thrown away in {@link
     * #detachModelProvider(String)}.
     *
     * <p>NOTE: The head and attached sessions will explicitly not be returned.
     */
    private Collection<Session> populateUnboundSessions() {
        threadUtils.checkNotMainThread();

        List<StreamSession> sessionList = getPersistedSessions();
        HeadSessionImpl headSession = Validators.checkNotNull(head);
        String headSessionId = headSession.getSessionId();

        Map<String, Session> unboundSessions = new HashMap<>();
        for (StreamSession session : sessionList) {
            String sessionId = session.getSessionId();
            synchronized (lock) {
                // We need only unbound sessions, i.e. sessions whose IDs are part of
                // sessionsMetadata but are not attached or head (those are always stored in
                // attachedSessions/head respectively so do not need to be restored from persistent
                // storage.
                if (attachedSessions.containsKey(sessionId)
                        || !sessionsMetadata.containsKey(sessionId)
                        || sessionId.equals(headSessionId)) {
                    continue;
                }
            }
            InitializableSession unboundSession;
            // Unbound sessions are sessions that are able to be created through restore
            unboundSession = sessionFactory.getSession();
            unboundSession.setSessionId(sessionId);
            unboundSessions.put(sessionId, unboundSession);

            // Populate the newly created unbound session.
            Logger.i(TAG, "Populate unbound session %s", sessionId);
            Result<List<StreamStructure>> streamStructuresResult =
                    store.getStreamStructures(sessionId);
            if (streamStructuresResult.isSuccessful()) {
                unboundSession.populateModelProvider(streamStructuresResult.getValue(),
                        /* cachedBindings= */ false,
                        /* legacyHeadContent= */ false, UiContext.getDefaultInstance());
            } else {
                Logger.e(TAG, "Failed to read unbound session state, ignored");
            }
        }

        return unboundSessions.values();
    }

    private void initializePersistedSessions(List<StreamSession> sessionList) {
        threadUtils.checkNotMainThread();

        HeadSessionImpl headSession = Validators.checkNotNull(head);
        String headSessionId = headSession.getSessionId();
        boolean cleanupSessions = false;
        for (StreamSession session : sessionList) {
            SessionMetadata metadata = getOrCreateSessionMetadata(session);
            String sessionId = session.getSessionId();
            if (sessionId.equals(headSessionId)) {
                // update the information stored for the $HEAD record we created previously
                Logger.i(TAG, "Updating $HEAD state, lastAdded %s",
                        StringFormattingUtils.formatLogDate(metadata.getLastAddedTimeMillis()));
                synchronized (lock) {
                    sessionsMetadata.put(sessionId, metadata);
                }
                continue;
            }
            if (!isSessionAlive(sessionId, metadata)) {
                Logger.i(TAG, "Found expired session %s, created %s", session.getSessionId(),
                        StringFormattingUtils.formatLogDate(metadata.getCreationTimeMillis()));
                cleanupSessions = true;
                continue;
            }
            synchronized (lock) {
                if (sessionsMetadata.containsKey(session.getSessionId())) {
                    // Don't replace sessions already found in sessions
                    continue;
                }
                sessionsMetadata.put(session.getSessionId(), metadata);
            }
        }

        if (cleanupSessions) {
            // Queue up a task to clear the session journals.
            taskQueue.execute(Task.CLEAN_UP_SESSION_JOURNALS, TaskType.BACKGROUND,
                    this::cleanupSessionJournals);
        }
        Set<String> reservedContentIds = new HashSet<>();
        reservedContentIds.add(STREAM_SESSION_CONTENT_ID);
        reservedContentIds.add(CONSISTENCY_TOKEN_CONTENT_ID);
        taskQueue.execute(Task.GARBAGE_COLLECT_CONTENT, TaskType.BACKGROUND,
                store.triggerContentGc(reservedContentIds, getAccessibleContentSupplier(),
                        shouldKeepSharedStates()));
    }

    private boolean shouldKeepSharedStates() {
        synchronized (lock) {
            for (String sessionId : sessionsMetadata.keySet()) {
                SessionMetadata metadata = sessionsMetadata.get(sessionId);
                if (metadata.getSchemaVersion()
                        < MIN_SCHEMA_VERSION_FOR_PIET_SHARED_STATE_REQUIRED_CONTENT) {
                    return true;
                }
            }
        }

        return false;
    }

    private Supplier<Set<String>> getAccessibleContentSupplier() {
        return () -> {
            threadUtils.checkNotMainThread();
            Logger.i(TAG, "Determining accessible content");

            // SessionIds should be determined at the time GC runs.
            Set<String> sessionIds;
            synchronized (lock) {
                sessionIds = sessionsMetadata.keySet();
            }

            Set<String> accessibleContent = new HashSet<>(head.getContentInSession());
            for (String sessionId : sessionIds) {
                if (sessionId.equals(head.getSessionId())) {
                    continue;
                }

                SessionContentTracker sessionContentTracker =
                        new SessionContentTracker(/* supportsClearAll= */ false);
                Result<List<StreamStructure>> streamStructuresResult =
                        store.getStreamStructures(sessionId);
                if (streamStructuresResult.isSuccessful()) {
                    sessionContentTracker.update(streamStructuresResult.getValue());
                } else {
                    Logger.e(TAG, "Failed to read unbound session state, ignored");
                }
                accessibleContent.addAll(sessionContentTracker.getContentIds());
            }
            return accessibleContent;
        };
    }

    @VisibleForTesting
    List<StreamSession> getPersistedSessions() {
        List<String> sessionIds = new ArrayList<>();
        sessionIds.add(STREAM_SESSION_CONTENT_ID);
        Result<List<PayloadWithId>> sessionPayloadResult = store.getPayloads(sessionIds);
        if (!sessionPayloadResult.isSuccessful()) {
            // If we cant read the persisted sessions, report the error and return null. No sessions
            // will be created, this is as if we deleted all existing sessions.  It should be safe
            // to ignore the error.
            Logger.e(TAG, "getPayloads failed to read the Persisted sessions");
            return Collections.emptyList();
        }

        List<PayloadWithId> sessionPayload = sessionPayloadResult.getValue();
        if (sessionPayload.isEmpty()) {
            Logger.w(TAG, "Persisted Sessions were not found");
            return Collections.emptyList();
        }
        StreamPayload payload = sessionPayload.get(0).payload;
        if (!payload.hasStreamSessions()) {
            Logger.e(TAG, "Persisted Sessions StreamSessions was not set");
            return Collections.emptyList();
        }
        return payload.getStreamSessions().getStreamSessionList();
    }

    @VisibleForTesting
    void updatePersistedSessionsMetadata() {
        threadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
        StreamSessions.Builder sessionsBuilder = StreamSessions.newBuilder();
        int sessionCount;
        synchronized (lock) {
            sessionCount = sessionsMetadata.size();
            for (String sessionId : sessionsMetadata.keySet()) {
                SessionMetadata sessionMetadata = sessionsMetadata.get(sessionId);
                if (sessionMetadata != null) {
                    sessionsBuilder.addStreamSession(
                            StreamSession.newBuilder().setSessionId(sessionId).setSessionMetadata(
                                    sessionMetadata));
                }
            }
        }
        store.editContent()
                .add(STREAM_SESSION_CONTENT_ID,
                        StreamPayload.newBuilder().setStreamSessions(sessionsBuilder).build())
                .commit();
        timeTracker.stop(
                "task", "updatePersistedSessionsMetadata(Content)", "sessionCount", sessionCount);
    }

    /**
     * Remove all session journals which are not currently found in {@code sessionsMetadata} which
     * contains all of the known sessions. This is a garbage collection task.
     */
    @VisibleForTesting
    void cleanupSessionJournals() {
        threadUtils.checkNotMainThread();
        Logger.i(TAG, "Task: cleanupSessionJournals");
        ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
        int sessionCleared = expiredSessionsCleared;

        Result<List<String>> storedSessionsResult = store.getAllSessions();
        if (storedSessionsResult.isSuccessful()) {
            synchronized (lock) {
                for (String sessionId : storedSessionsResult.getValue()) {
                    if (!sessionsMetadata.containsKey(sessionId)) {
                        store.removeSession(sessionId);
                        expiredSessionsCleared++;
                    }
                }
            }
        } else {
            // We were unable to read all the sessions, log an error and then ignore the fact that
            // cleanup failed.
            Logger.e(TAG, "Error reading all sessions, Unable to cleanup session journals");
        }
        timeTracker.stop("task", "cleanupSessionJournals", "sessionsCleared",
                expiredSessionsCleared - sessionCleared);
    }

    @VisibleForTesting
    boolean isSessionAlive(String sessionId, SessionMetadata metadata) {
        // Today HEAD will does not time out
        return head.getSessionId().equals(sessionId)
                || (metadata.getCreationTimeMillis() + lifetimeMs) > clock.currentTimeMillis();
    }

    private SessionMetadata getOrCreateSessionMetadata(StreamSession streamSession) {
        if (streamSession.hasSessionMetadata()) {
            return streamSession.getSessionMetadata();
        }

        // TODO: Log to BasicLoggingApi that SessionMetadata is missing.
        SessionMetadata.Builder metadataBuilder = SessionMetadata.newBuilder();
        if (streamSession.getSessionId().equals(head.getSessionId())) {
            metadataBuilder.setLastAddedTimeMillis(streamSession.getLegacyTimeMillis());
        } else {
            metadataBuilder.setCreationTimeMillis(streamSession.getLegacyTimeMillis());
        }
        return metadataBuilder.build();
    }

    private int getHeadSchemaVersion(List<StreamSession> sessionList) {
        for (StreamSession streamSession : sessionList) {
            if (streamSession.getSessionId().equals(head.getSessionId())) {
                return streamSession.getSessionMetadata().getSchemaVersion();
            }
        }

        return 0;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        synchronized (lock) {
            dumper.forKey("attached sessions").value(attachedSessions.size());
        }
        dumper.forKey("expiredSessionsCleared").value(expiredSessionsCleared).compactPrevious();
        dumper.forKey("unboundSessionCount").value(unboundSessionCount).compactPrevious();
        dumper.forKey("detachedSessionCount").value(detachedSessionCount).compactPrevious();
        dumper.forKey("get").value(getCount);
        dumper.forKey("getAttached").value(getAttachedCount).compactPrevious();
        dumper.forKey("getAll").value(getAllCount).compactPrevious();
        dumper.forKey("put").value(putCount).compactPrevious();
        dumper.forKey("remove").value(removeCount).compactPrevious();
    }
}
