// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.logging.StringFormattingUtils;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils.ElapsedTimeTracker;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.SessionMetadata;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSession;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSessions;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

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
     * org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage}
     */
    public static final String CONSISTENCY_TOKEN_CONTENT_ID = "ct";

    // Used to synchronize the stored data
    private final Object mLock = new Object();

    // Sessions with ModelProviders (attached).
    @GuardedBy("mLock")
    private final Map<String, Session> mAttachedSessions = new HashMap<>();

    @GuardedBy("mLock")
    private final Map<String, SessionMetadata> mSessionsMetadata = new HashMap<>();

    private final HeadSessionImpl mHead;

    private final Store mStore;
    private final TaskQueue mTaskQueue;
    private final SessionFactory mSessionFactory;
    private final long mLifetimeMs;
    private final TimingUtils mTimingUtils;
    private final ThreadUtils mThreadUtils;
    private final Clock mClock;

    private boolean mInitialized;

    // operation counts for the dumper
    private int mGetCount;
    private int mGetAttachedCount;
    private int mGetAllCount;
    private int mPutCount;
    private int mRemoveCount;
    private int mUnboundSessionCount;
    private int mDetachedSessionCount;
    private int mExpiredSessionsCleared;

    public SessionCache(Store store, TaskQueue taskQueue, SessionFactory sessionFactory,
            long lifetimeMs, TimingUtils timingUtils, ThreadUtils threadUtil, Clock clock) {
        this.mStore = store;
        this.mTaskQueue = taskQueue;
        this.mSessionFactory = sessionFactory;
        this.mLifetimeMs = lifetimeMs;
        this.mTimingUtils = timingUtils;
        this.mThreadUtils = threadUtil;
        this.mClock = clock;

        this.mHead = sessionFactory.getHeadSession();
    }

    /**
     * Return {@link HeadSessionImpl} for head. Returns {@code null} if {@link #initialize()} hasn't
     * finished running.
     */
    public HeadSessionImpl getHead() {
        return mHead;
    }

    /** Returns {@code true} if HEAD has been initialize, which happens in {@link #initialize()}. */
    public boolean isHeadInitialized() {
        return mInitialized;
    }

    /**
     * Returns true if sessionId exists in any of {attached, unbound, head} set, false otherwise.
     */
    public boolean hasSession(String sessionId) {
        synchronized (mLock) {
            return mSessionsMetadata.containsKey(sessionId);
        }
    }

    /** Returns all attached sessions tracked by the SessionCache. This does NOT include head. */
    public List<Session> getAttachedSessions() {
        mGetAttachedCount++;
        synchronized (mLock) {
            Logger.d(TAG, "getAttachedSessions, size=%d", mAttachedSessions.size());
            return new ArrayList<>(mAttachedSessions.values());
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
        mThreadUtils.checkNotMainThread();
        mGetAllCount++;

        // Re-build the unbound sessions (that were thrown away on detach) and add them.
        List<Session> allSessions = new ArrayList<>(populateUnboundSessions());

        allSessions.add(mHead);
        synchronized (mLock) {
            allSessions.addAll(mAttachedSessions.values());
        }

        Logger.d(TAG, "getAllSessions, size=%d", allSessions.size());
        return allSessions;
    }

    /**
     * Return an attached {@link Session} for the sessionId, or {@code null} if the sessionCache
     * doesn't contain the Session or it is no longer attached.
     */
    @Nullable
    public Session getAttached(String sessionId) {
        mGetCount++;
        synchronized (mLock) {
            return mAttachedSessions.get(sessionId);
        }
    }

    /** Returns the last time content was added to HEAD. */
    public long getHeadLastAddedTimeMillis() {
        synchronized (mLock) {
            SessionMetadata metadata = mSessionsMetadata.get(mHead.getSessionId());
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

        mThreadUtils.checkNotMainThread();
        mPutCount++;
        synchronized (mLock) {
            mAttachedSessions.put(sessionId, session);
            mSessionsMetadata.put(sessionId,
                    SessionMetadata.newBuilder()
                            .setCreationTimeMillis(creationTimeMillis)
                            .setSchemaVersion(schemaVersion)
                            .build());
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", mAttachedSessions.size(),
                    mSessionsMetadata.size());
        }
        updatePersistedSessionsMetadata();
    }

    /** Add a {@link Session} to the SessionCache and retain the existing metadata. */
    public void putAttachedAndRetainMetadata(String sessionId, Session session) {
        Logger.d(TAG, "putAttachedAndRetainMetadata, sessionId=%s", sessionId);

        mThreadUtils.checkNotMainThread();
        mPutCount++;
        synchronized (mLock) {
            mAttachedSessions.put(sessionId, session);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", mAttachedSessions.size(),
                    mSessionsMetadata.size());
        }
    }

    /**
     * Detaches the model provider from the session, which becomes unbound. We throw away unbound
     * sessions for memory reasons, they are re-created on demand when needed in {@link
     * #populateUnboundSessions()}.
     */
    public void detachModelProvider(String sessionId) {
        Logger.d(TAG, "detachModelProvider, sessionId=%s", sessionId);

        mThreadUtils.checkNotMainThread();
        InitializableSession initializableSession;
        synchronized (mLock) {
            Session session = mAttachedSessions.get(sessionId);
            if (!(session instanceof InitializableSession)) {
                Logger.w(TAG, "Unable to detach session %s", sessionId);
                return;
            } else {
                initializableSession = (InitializableSession) session;
            }
            mAttachedSessions.remove(sessionId);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", mAttachedSessions.size(),
                    mSessionsMetadata.size());
        }

        initializableSession.bindModelProvider(null, null);
        mDetachedSessionCount++;
    }

    /** Remove an attached {@link Session} from the SessionCache. */
    public void removeAttached(String sessionId) {
        Logger.d(TAG, "removeAttached, sessionId=%s", sessionId);

        mThreadUtils.checkNotMainThread();
        mRemoveCount++;
        synchronized (mLock) {
            mAttachedSessions.remove(sessionId);
            mSessionsMetadata.remove(sessionId);
            Logger.d(TAG, "Sessions size: attached=%d, all=%d", mAttachedSessions.size(),
                    mSessionsMetadata.size());
        }
        updatePersistedSessionsMetadata();
    }

    /** Initialize the SessionCache from Store. */
    public boolean initialize() {
        mThreadUtils.checkNotMainThread();

        // create the head session from the data in the Store
        ElapsedTimeTracker headTimeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        Result<List<StreamStructure>> results = mStore.getStreamStructures(mHead.getSessionId());
        if (!results.isSuccessful()) {
            Logger.w(TAG, "unable to get head stream structures");
            return false;
        }

        List<StreamSession> sessionList = getPersistedSessions();
        int headSchemaVersion = getHeadSchemaVersion(sessionList);
        mHead.initializeSession(results.getValue(), headSchemaVersion);
        mInitialized = true;
        headTimeTracker.stop("", "createHead");

        initializePersistedSessions(sessionList);

        // Ensure that SessionMetadata exists for HEAD.
        synchronized (mLock) {
            if (!mSessionsMetadata.containsKey(mHead.getSessionId())) {
                mSessionsMetadata.put(mHead.getSessionId(), SessionMetadata.getDefaultInstance());
            }
            Logger.d(TAG, "initialize, size=%d", mSessionsMetadata.size());
        }
        return true;
    }

    public void reset() {
        List<Session> sessionList;
        synchronized (mLock) {
            sessionList = new ArrayList<>(mAttachedSessions.values());
        }
        for (Session session : sessionList) {
            ModelProvider modelProvider = session.getModelProvider();
            if (modelProvider != null) {
                modelProvider.invalidate();
            }
        }
        synchronized (mLock) {
            mAttachedSessions.clear();
            mSessionsMetadata.clear();
            mHead.reset();
            mSessionsMetadata.put(mHead.getSessionId(), SessionMetadata.getDefaultInstance());
            mInitialized = true;
        }
    }

    /** Returns when the specified {@code sessionId} was created. */
    @VisibleForTesting
    long getCreationTimeMillis(String sessionId) {
        synchronized (mLock) {
            SessionMetadata metadata = mSessionsMetadata.get(sessionId);
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
        mThreadUtils.checkNotMainThread();
        synchronized (mLock) {
            SessionMetadata metadata = mSessionsMetadata.get(mHead.getSessionId());
            SessionMetadata.Builder builder =
                    metadata == null ? SessionMetadata.newBuilder() : metadata.toBuilder();
            mSessionsMetadata.put(mHead.getSessionId(),
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
        mThreadUtils.checkNotMainThread();

        List<StreamSession> sessionList = getPersistedSessions();
        HeadSessionImpl headSession = Validators.checkNotNull(mHead);
        String headSessionId = headSession.getSessionId();

        Map<String, Session> unboundSessions = new HashMap<>();
        for (StreamSession session : sessionList) {
            String sessionId = session.getSessionId();
            synchronized (mLock) {
                // We need only unbound sessions, i.e. sessions whose IDs are part of
                // sessionsMetadata but are not attached or head (those are always stored in
                // attachedSessions/head respectively so do not need to be restored from persistent
                // storage.
                if (mAttachedSessions.containsKey(sessionId)
                        || !mSessionsMetadata.containsKey(sessionId)
                        || sessionId.equals(headSessionId)) {
                    continue;
                }
            }
            InitializableSession unboundSession;
            // Unbound sessions are sessions that are able to be created through restore
            unboundSession = mSessionFactory.getSession();
            unboundSession.setSessionId(sessionId);
            unboundSessions.put(sessionId, unboundSession);

            // Populate the newly created unbound session.
            Logger.i(TAG, "Populate unbound session %s", sessionId);
            Result<List<StreamStructure>> streamStructuresResult =
                    mStore.getStreamStructures(sessionId);
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
        mThreadUtils.checkNotMainThread();

        HeadSessionImpl headSession = Validators.checkNotNull(mHead);
        String headSessionId = headSession.getSessionId();
        boolean cleanupSessions = false;
        for (StreamSession session : sessionList) {
            SessionMetadata metadata = getOrCreateSessionMetadata(session);
            String sessionId = session.getSessionId();
            if (sessionId.equals(headSessionId)) {
                // update the information stored for the $HEAD record we created previously
                Logger.i(TAG, "Updating $HEAD state, lastAdded %s",
                        StringFormattingUtils.formatLogDate(metadata.getLastAddedTimeMillis()));
                synchronized (mLock) {
                    mSessionsMetadata.put(sessionId, metadata);
                }
                continue;
            }
            if (!isSessionAlive(sessionId, metadata)) {
                Logger.i(TAG, "Found expired session %s, created %s", session.getSessionId(),
                        StringFormattingUtils.formatLogDate(metadata.getCreationTimeMillis()));
                cleanupSessions = true;
                continue;
            }
            synchronized (mLock) {
                if (mSessionsMetadata.containsKey(session.getSessionId())) {
                    // Don't replace sessions already found in sessions
                    continue;
                }
                mSessionsMetadata.put(session.getSessionId(), metadata);
            }
        }

        if (cleanupSessions) {
            // Queue up a task to clear the session journals.
            mTaskQueue.execute(Task.CLEAN_UP_SESSION_JOURNALS, TaskType.BACKGROUND,
                    this::cleanupSessionJournals);
        }
        Set<String> reservedContentIds = new HashSet<>();
        reservedContentIds.add(STREAM_SESSION_CONTENT_ID);
        reservedContentIds.add(CONSISTENCY_TOKEN_CONTENT_ID);
        mTaskQueue.execute(Task.GARBAGE_COLLECT_CONTENT, TaskType.BACKGROUND,
                mStore.triggerContentGc(reservedContentIds, getAccessibleContentSupplier(),
                        shouldKeepSharedStates()));
    }

    private boolean shouldKeepSharedStates() {
        synchronized (mLock) {
            for (String sessionId : mSessionsMetadata.keySet()) {
                SessionMetadata metadata = mSessionsMetadata.get(sessionId);
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
            mThreadUtils.checkNotMainThread();
            Logger.i(TAG, "Determining accessible content");

            // SessionIds should be determined at the time GC runs.
            Set<String> sessionIds;
            synchronized (mLock) {
                sessionIds = mSessionsMetadata.keySet();
            }

            Set<String> accessibleContent = new HashSet<>(mHead.getContentInSession());
            for (String sessionId : sessionIds) {
                if (sessionId.equals(mHead.getSessionId())) {
                    continue;
                }

                SessionContentTracker sessionContentTracker =
                        new SessionContentTracker(/* supportsClearAll= */ false);
                Result<List<StreamStructure>> streamStructuresResult =
                        mStore.getStreamStructures(sessionId);
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
        Result<List<PayloadWithId>> sessionPayloadResult = mStore.getPayloads(sessionIds);
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
        mThreadUtils.checkNotMainThread();
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        StreamSessions.Builder sessionsBuilder = StreamSessions.newBuilder();
        int sessionCount;
        synchronized (mLock) {
            sessionCount = mSessionsMetadata.size();
            for (String sessionId : mSessionsMetadata.keySet()) {
                SessionMetadata sessionMetadata = mSessionsMetadata.get(sessionId);
                if (sessionMetadata != null) {
                    sessionsBuilder.addStreamSession(
                            StreamSession.newBuilder().setSessionId(sessionId).setSessionMetadata(
                                    sessionMetadata));
                }
            }
        }
        mStore.editContent()
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
        mThreadUtils.checkNotMainThread();
        Logger.i(TAG, "Task: cleanupSessionJournals");
        ElapsedTimeTracker timeTracker = mTimingUtils.getElapsedTimeTracker(TAG);
        int sessionCleared = mExpiredSessionsCleared;

        Result<List<String>> storedSessionsResult = mStore.getAllSessions();
        if (storedSessionsResult.isSuccessful()) {
            synchronized (mLock) {
                for (String sessionId : storedSessionsResult.getValue()) {
                    if (!mSessionsMetadata.containsKey(sessionId)) {
                        mStore.removeSession(sessionId);
                        mExpiredSessionsCleared++;
                    }
                }
            }
        } else {
            // We were unable to read all the sessions, log an error and then ignore the fact that
            // cleanup failed.
            Logger.e(TAG, "Error reading all sessions, Unable to cleanup session journals");
        }
        timeTracker.stop("task", "cleanupSessionJournals", "sessionsCleared",
                mExpiredSessionsCleared - sessionCleared);
    }

    @VisibleForTesting
    boolean isSessionAlive(String sessionId, SessionMetadata metadata) {
        // Today HEAD will does not time out
        return mHead.getSessionId().equals(sessionId)
                || (metadata.getCreationTimeMillis() + mLifetimeMs) > mClock.currentTimeMillis();
    }

    private SessionMetadata getOrCreateSessionMetadata(StreamSession streamSession) {
        if (streamSession.hasSessionMetadata()) {
            return streamSession.getSessionMetadata();
        }

        // TODO: Log to BasicLoggingApi that SessionMetadata is missing.
        SessionMetadata.Builder metadataBuilder = SessionMetadata.newBuilder();
        if (streamSession.getSessionId().equals(mHead.getSessionId())) {
            metadataBuilder.setLastAddedTimeMillis(streamSession.getLegacyTimeMillis());
        } else {
            metadataBuilder.setCreationTimeMillis(streamSession.getLegacyTimeMillis());
        }
        return metadataBuilder.build();
    }

    private int getHeadSchemaVersion(List<StreamSession> sessionList) {
        for (StreamSession streamSession : sessionList) {
            if (streamSession.getSessionId().equals(mHead.getSessionId())) {
                return streamSession.getSessionMetadata().getSchemaVersion();
            }
        }

        return 0;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        synchronized (mLock) {
            dumper.forKey("attached sessions").value(mAttachedSessions.size());
        }
        dumper.forKey("expiredSessionsCleared").value(mExpiredSessionsCleared).compactPrevious();
        dumper.forKey("unboundSessionCount").value(mUnboundSessionCount).compactPrevious();
        dumper.forKey("detachedSessionCount").value(mDetachedSessionCount).compactPrevious();
        dumper.forKey("get").value(mGetCount);
        dumper.forKey("getAttached").value(mGetAttachedCount).compactPrevious();
        dumper.forKey("getAll").value(mGetAllCount).compactPrevious();
        dumper.forKey("put").value(mPutCount).compactPrevious();
        dumper.forKey("remove").value(mRemoveCount).compactPrevious();
    }
}
