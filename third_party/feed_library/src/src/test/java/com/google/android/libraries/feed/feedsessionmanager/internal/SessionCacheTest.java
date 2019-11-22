// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.testing.FakeDirectExecutor;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelProvider;
import com.google.android.libraries.feed.testing.store.FakeStore;
import com.google.search.now.feed.client.StreamDataProto.SessionMetadata;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSession;
import com.google.search.now.feed.client.StreamDataProto.StreamSessions;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link SessionCache} class. */
@RunWith(RobolectricTestRunner.class)
public class SessionCacheTest {
    private static final long DEFAULT_LIFETIME_MS = 10;
    private static final int SCHEMA_VERSION = 4;

    private final Configuration configuration = new Configuration.Builder().build();
    private final ContentIdGenerators idGenerators = new ContentIdGenerators();
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils timingUtils = new TimingUtils();

    private FakeStore fakeStore;
    private FakeTaskQueue fakeTaskQueue;
    private SessionFactory sessionFactory;
    private SessionCache sessionCache;

    protected final FakeModelProvider fakeModelProvider = new FakeModelProvider();

    @Before
    public void setUp() {
        initMocks(this);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeStore = new FakeStore(configuration, fakeThreadUtils, fakeTaskQueue, fakeClock);
        fakeThreadUtils.enforceMainThread(false);
        fakeTaskQueue.initialize(() -> {});
        sessionCache = getSessionCache();
    }

    @Test
    public void testInitialization() {
        int schemaVersion = 3;
        populateHead();
        mockStreamSessions(
                StreamSessions.newBuilder()
                        .addStreamSession(
                                StreamSession.newBuilder()
                                        .setSessionId(Store.HEAD_SESSION_ID)
                                        .setSessionMetadata(
                                                SessionMetadata.newBuilder().setSchemaVersion(
                                                        schemaVersion)))
                        .build());
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        assertThat(sessionCache.isHeadInitialized()).isFalse();
        assertThat(sessionCache.getHead()).isNotNull();
        assertThat(sessionCache.getHead().isHeadEmpty()).isTrue();
        sessionCache.initialize();

        // Initialization adds $HEAD
        assertThat(sessionCache.isHeadInitialized()).isTrue();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        assertThat(sessionCache.getHead()).isNotNull();
        assertThat(sessionCache.getHead().isHeadEmpty()).isFalse();
        assertThat(sessionCache.getHead().getSchemaVersion()).isEqualTo(schemaVersion);
    }

    @Test
    public void testInitialization_accessibleContentShouldBeDeterminedAtGcTime() {
        FakeDirectExecutor fakeDirectExecutor = FakeDirectExecutor.queueAllTasks(fakeThreadUtils);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeDirectExecutor);
        fakeStore = new FakeStore(configuration, fakeThreadUtils, fakeTaskQueue, fakeClock);
        fakeThreadUtils.enforceMainThread(false);
        fakeTaskQueue.initialize(() -> {});
        sessionCache = getSessionCache();

        Session session = populateSession(1, 2, /* commitToStore= */ true);
        sessionCache.initialize();
        setSessions(session);
        fakeDirectExecutor.runAllTasks(); // Run GC.

        assertThat(fakeDirectExecutor.hasTasks()).isFalse();
        assertPayloads(/* featureCnt= */ 2);
    }

    @Test
    public void testInitialization_contentGcShouldKeepSharedStates() {
        fakeStore.setContent("foo",
                StreamPayload.newBuilder()
                        .setStreamSharedState(
                                StreamSharedState.newBuilder().setContentId("foo").build())
                        .build());
        mockStreamSessions(
                StreamSessions.newBuilder()
                        .addStreamSession(
                                StreamSession.newBuilder()
                                        .setSessionId(Store.HEAD_SESSION_ID)
                                        .setSessionMetadata(
                                                // clang-format off
          SessionMetadata.newBuilder().setSchemaVersion(
            SessionCache.MIN_SCHEMA_VERSION_FOR_PIET_SHARED_STATE_REQUIRED_CONTENT - 1)
                                                // clang-format on
                                                ))
                        .build());

        assertThat(fakeStore.getSharedStates().getValue()).hasSize(1);
        sessionCache.initialize();
        assertThat(fakeStore.getSharedStates().getValue()).hasSize(1);
    }

    @Test
    public void testInitialization_contentGcShouldDiscardSharedStates() {
        fakeStore.setContent("foo",
                StreamPayload.newBuilder()
                        .setStreamSharedState(
                                StreamSharedState.newBuilder().setContentId("foo").build())
                        .build());
        mockStreamSessions(
                StreamSessions.newBuilder()
                        .addStreamSession(
                                StreamSession.newBuilder()
                                        .setSessionId(Store.HEAD_SESSION_ID)
                                        .setSessionMetadata(
                                                // clang-format off
                                          SessionMetadata.newBuilder().setSchemaVersion(
                    SessionCache.MIN_SCHEMA_VERSION_FOR_PIET_SHARED_STATE_REQUIRED_CONTENT)
                                                // clang-format on
                                                ))
                        .build());

        assertThat(fakeStore.getSharedStates().getValue()).hasSize(1);
        sessionCache.initialize();
        assertThat(fakeStore.getSharedStates().getValue()).isEmpty();
    }

    @Test
    public void testPutGet() {
        sessionCache.initialize();
        Session session = populateSession(1, 2);
        sessionCache.putAttachedAndRetainMetadata(session.getSessionId(), session);

        Session ret = sessionCache.getAttached(session.getSessionId());
        assertThat(ret).isEqualTo(session);
    }

    @Test
    public void testPut_persisted() {
        sessionCache.initialize();
        Session session = populateSession(1, 2);
        sessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        Session ret = sessionCache.getAttached(session.getSessionId());
        assertThat(ret).isEqualTo(session);

        session = populateSession(2, 2);
        sessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        List<StreamSession> streamSessionList = sessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(3);
    }

    @Test
    public void testRemove() {
        sessionCache.initialize();
        Session session = populateSession(1, 2);
        sessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        List<StreamSession> streamSessionList = sessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(2);

        String id = session.getSessionId();
        sessionCache.removeAttached(id);
        assertThat(sessionCache.getAttached(id)).isNull();

        streamSessionList = sessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(1);
    }

    @Test
    public void testDetach() {
        sessionCache.initialize();
        Session s1 = populateSession(1, 2);
        String s1Id = s1.getSessionId();
        sessionCache.putAttachedAndRetainMetadata(s1Id, s1);

        List<Session> sessions = sessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(1);
        assertThat(sessions).contains(s1);
        assertThat(s1.getModelProvider()).isNotNull();

        sessionCache.detachModelProvider(s1Id);
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        assertThat(s1.getModelProvider()).isNull();
    }

    @Test
    public void testGetAttachedSessions() {
        sessionCache.initialize();
        Session s1 = populateSession(1, 2);
        sessionCache.putAttachedAndRetainMetadata(s1.getSessionId(), s1);
        Session s2 = populateSession(2, 2);
        sessionCache.putAttachedAndRetainMetadata(s2.getSessionId(), s2);

        List<Session> sessions = sessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(2);
        assertThat(sessions).contains(sessionCache.getAttached(s1.getSessionId()));
        assertThat(sessions).contains(sessionCache.getAttached(s2.getSessionId()));
    }

    @Test
    public void testGetAllSessions() {
        sessionCache.initialize();
        Session headSession = sessionCache.getHead();

        Session s1 = populateSession(1, 2, /* commitToStore= */ true);
        String s1Id = s1.getSessionId();
        sessionCache.putAttached(s1Id, 1L, SCHEMA_VERSION, s1);

        assertThat(sessionCache.getAttachedSessions()).containsExactly(s1);
        assertThat(sessionCache.getAllSessions()).containsExactly(s1, headSession);

        Session s2 = populateSession(2, 2, /* commitToStore= */ true);
        String s2Id = s2.getSessionId();
        sessionCache.putAttached(s2Id, 2L, SCHEMA_VERSION, s2);

        assertThat(sessionCache.getAttachedSessions()).containsExactly(s1, s2);
        assertThat(sessionCache.getAllSessions()).containsExactly(s1, s2, headSession);

        // Detach the session, which will throw it away from SessionCache.
        sessionCache.detachModelProvider(s1Id);
        assertThat(sessionCache.getAttachedSessions()).containsExactly(s2);

        List<Session> allSessions = new ArrayList<>(sessionCache.getAllSessions());
        assertThat(allSessions).hasSize(3);
        assertThat(allSessions).containsAtLeast(s2, headSession);
        allSessions.remove(s2);
        allSessions.remove(headSession);

        // A new unbound session was created for the detached one, with the same ID.
        Session unboundSession = allSessions.get(0);
        assertThat(unboundSession.getSessionId()).isEqualTo(s1.getSessionId());
        assertThat(unboundSession.getContentInSession()).isEqualTo(s1.getContentInSession());
        assertThat(unboundSession).isNotSameInstanceAs(s1);
    }

    @Test
    public void testReset_headOnly() {
        sessionCache.initialize();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();

        sessionCache.reset();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
    }

    @Test
    public void testReset_sessions() {
        sessionCache.initialize();
        int sessionCount = 2;
        for (int i = 0; i < sessionCount; i++) {
            Session session = populateSession(i, 2);
            sessionCache.putAttachedAndRetainMetadata(session.getSessionId(), session);
        }
        List<Session> sessions = sessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(sessionCount);

        sessionCache.reset();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
    }

    @Test
    public void testIsSessionAlive() {
        sessionCache.initialize();
        fakeClock.set(DEFAULT_LIFETIME_MS + 2);
        assertThat(sessionCache.isSessionAlive("stream:1", SessionMetadata.getDefaultInstance()))
                .isFalse();
        assertThat(sessionCache.isSessionAlive(
                           Store.HEAD_SESSION_ID, SessionMetadata.getDefaultInstance()))
                .isTrue();
        assertThat(sessionCache.isSessionAlive("stream:2",
                           SessionMetadata.newBuilder()
                                   .setCreationTimeMillis(DEFAULT_LIFETIME_MS - 1)
                                   .build()))
                .isTrue();
    }

    @Test
    public void testGetPersistedSessions() {
        StreamSession streamSession = StreamSession.getDefaultInstance();
        mockStreamSessions(StreamSessions.newBuilder().addStreamSession(streamSession).build());

        List<StreamSession> sessionList = sessionCache.getPersistedSessions();
        assertThat(sessionList).containsExactly(streamSession);
    }

    @Test
    public void testCleanupJournals() {
        String sessionId1 = "stream:1";
        String sessionId2 = "stream:2";
        fakeStore.setStreamStructures(sessionId1, StreamStructure.getDefaultInstance())
                .setStreamStructures(sessionId2, StreamStructure.getDefaultInstance());

        Session s2 = mock(Session.class);
        when(s2.getSessionId()).thenReturn(sessionId2);
        setSessions(s2);
        sessionCache.cleanupSessionJournals();
        assertThat(fakeStore.getAllSessions().getValue()).containsExactly(sessionId2);
        assertThat(sessionCache.getAttached(sessionId2)).isEqualTo(s2);
    }

    @Test
    public void testInitializePersistedSessions_emptyStreamSessions() {
        fakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(StreamSessions.getDefaultInstance());
        sessionCache.initialize();

        assertThat(sessionCache.getAttachedSessions()).isEmpty();
    }

    @Test
    public void testInitializePersistedSessions_legacyStreamSession() {
        StreamSessions streamSessions =
                StreamSessions.newBuilder()
                        .addStreamSession(StreamSession.newBuilder()
                                                  .setSessionId("stream:1")
                                                  .setLegacyTimeMillis(0))
                        .addStreamSession(StreamSession.newBuilder()
                                                  .setSessionId("stream:2")
                                                  .setLegacyTimeMillis(DEFAULT_LIFETIME_MS - 1))
                        .addStreamSession(StreamSession.newBuilder()
                                                  .setSessionId(Store.HEAD_SESSION_ID)
                                                  .setLegacyTimeMillis(1))
                        .build();

        fakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(streamSessions);
        sessionCache.initialize();

        assertThat(sessionCache.hasSession("stream:1")).isFalse();
        assertThat(sessionCache.hasSession("stream:2")).isTrue();
        assertThat(sessionCache.getCreationTimeMillis("stream:2"))
                .isEqualTo(DEFAULT_LIFETIME_MS - 1);
        assertThat(sessionCache.getHeadLastAddedTimeMillis()).isEqualTo(1);
    }

    @Test
    public void testInitializePersistedSessions_sessionMetadata() {
        StreamSessions streamSessions =
                StreamSessions.newBuilder()
                        .addStreamSession(
                                StreamSession.newBuilder()
                                        .setSessionId("stream:1")
                                        .setSessionMetadata(SessionMetadata.getDefaultInstance()))
                        .addStreamSession(
                                StreamSession.newBuilder()
                                        .setSessionId("stream:2")
                                        .setSessionMetadata(
                                                SessionMetadata.newBuilder().setCreationTimeMillis(
                                                        DEFAULT_LIFETIME_MS - 1)))
                        .build();

        fakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(streamSessions);
        sessionCache.initialize();

        assertThat(sessionCache.hasSession("stream:1")).isFalse();
        assertThat(sessionCache.hasSession("stream:2")).isTrue();
        assertThat(sessionCache.getCreationTimeMillis("stream:2"))
                .isEqualTo(DEFAULT_LIFETIME_MS - 1);
    }

    @Test
    public void testUpdateHeadMetadata() {
        long currentTime = 2L;
        int schemaVersion = 3;
        StreamSessions streamSessions =
                StreamSessions.newBuilder()
                        .addStreamSession(StreamSession.newBuilder()
                                                  .setSessionId(Store.HEAD_SESSION_ID)
                                                  .setLegacyTimeMillis(1))
                        .build();

        mockStreamSessions(streamSessions);
        sessionCache.initialize();
        sessionCache.updateHeadMetadata(currentTime, schemaVersion);

        List<Object> content = fakeStore.getContentById(SessionCache.STREAM_SESSION_CONTENT_ID);
        assertThat(content).hasSize(1);
        assertThat(((PayloadWithId) content.get(0)).payload)
                .isEqualTo(
                        StreamPayload.newBuilder()
                                .setStreamSessions(StreamSessions.newBuilder().addStreamSession(
                                        StreamSession.newBuilder()
                                                .setSessionId(Store.HEAD_SESSION_ID)
                                                .setSessionMetadata(
                                                        SessionMetadata.newBuilder()
                                                                .setLastAddedTimeMillis(currentTime)
                                                                .setSchemaVersion(schemaVersion))))
                                .build());
    }

    @Test
    public void testUpdatePersistedSessions() {
        sessionCache.initialize();

        // persist HEAD into the store
        sessionCache.updatePersistedSessionsMetadata();
        List<StreamSession> streamSessionList = sessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(1);

        // add additional sessions
        StreamSession session1 =
                StreamSession.newBuilder()
                        .setSessionId("stream:1")
                        .setSessionMetadata(SessionMetadata.newBuilder()
                                                    .setCreationTimeMillis(0L)
                                                    .setSchemaVersion(SCHEMA_VERSION))
                        .build();
        StreamSession session2 =
                StreamSession.newBuilder()
                        .setSessionId("stream:2")
                        .setSessionMetadata(SessionMetadata.newBuilder()
                                                    .setCreationTimeMillis(0L)
                                                    .setSchemaVersion(SCHEMA_VERSION))
                        .build();
        Session s1 = mock(Session.class);
        when(s1.getSessionId()).thenReturn(session1.getSessionId());
        Session s2 = mock(Session.class);
        when(s2.getSessionId()).thenReturn(session2.getSessionId());
        setSessions(s1, s2);

        sessionCache.updatePersistedSessionsMetadata();
        streamSessionList = sessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(3);
        assertThat(streamSessionList).contains(session1);
        assertThat(streamSessionList).contains(session2);
    }

    @Test
    public void testErrors_persistedSession() {
        sessionCache.initialize();
        sessionCache.updatePersistedSessionsMetadata();

        fakeStore.setAllowGetPayloads(false);
        assertThat(sessionCache.getPersistedSessions()).isEmpty();
    }

    private SessionCache getSessionCache() {
        sessionFactory = new SessionFactory(
                fakeStore, fakeTaskQueue, timingUtils, fakeThreadUtils, configuration);
        return new SessionCache(fakeStore, fakeTaskQueue, sessionFactory, 10, timingUtils,
                fakeThreadUtils, fakeClock);
    }

    private void mockStreamSessions(StreamSessions streamSessions) {
        fakeStore.setContent(SessionCache.STREAM_SESSION_CONTENT_ID,
                StreamPayload.newBuilder().setStreamSessions(streamSessions).build());
    }

    private Session populateSession(int id, int featureCnt) {
        return populateSession(id, featureCnt, /* commitToStore= */ false);
    }

    private Session populateSession(int id, int featureCnt, boolean commitToStore) {
        InitializableSession session = sessionFactory.getSession();
        String rootId = idGenerators.createRootContentId(1);
        List<StreamStructure> head = new ArrayList<>();
        head.add(StreamStructure.newBuilder()
                         .setOperation(Operation.UPDATE_OR_APPEND)
                         .setContentId(rootId)
                         .build());
        for (int i = 0; i < featureCnt; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            head.add(StreamStructure.newBuilder()
                             .setOperation(Operation.UPDATE_OR_APPEND)
                             .setContentId(contentId)
                             .setParentContentId(rootId)
                             .build());
            if (commitToStore) {
                fakeStore.setContent(contentId,
                        StreamPayload.newBuilder()
                                .setStreamFeature(StreamFeature.getDefaultInstance())
                                .build());
            }
        }
        session.setSessionId("stream:" + id);
        session.bindModelProvider(fakeModelProvider, mock(ViewDepthProvider.class));
        session.populateModelProvider(head, true, false, UiContext.getDefaultInstance());

        if (commitToStore) {
            fakeStore.setStreamStructures(session.getSessionId(), head);
        }

        return session;
    }

    private void populateHead() {
        fakeStore.setStreamStructures(Store.HEAD_SESSION_ID,
                StreamStructure.newBuilder()
                        .setOperation(Operation.UPDATE_OR_APPEND)
                        .setContentId(idGenerators.createRootContentId(1))
                        .build());
    }

    private void setSessions(Session... testSessions) {
        for (Session session : testSessions) {
            sessionCache.putAttached(
                    session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);
        }
    }

    private void assertPayloads(int featureCnt) {
        for (int i = 0; i < featureCnt; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            assertThat(fakeStore.getContentById(contentId)).hasSize(1);
        }
    }
}
