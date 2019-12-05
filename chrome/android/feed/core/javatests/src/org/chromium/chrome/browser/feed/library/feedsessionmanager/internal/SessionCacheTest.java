// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeDirectExecutor;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelProvider;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.SessionMetadata;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSession;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSessions;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link SessionCache} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionCacheTest {
    private static final long DEFAULT_LIFETIME_MS = 10;
    private static final int SCHEMA_VERSION = 4;

    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils mTimingUtils = new TimingUtils();

    private FakeStore mFakeStore;
    private FakeTaskQueue mFakeTaskQueue;
    private SessionFactory mSessionFactory;
    private SessionCache mSessionCache;

    protected final FakeModelProvider mFakeModelProvider = new FakeModelProvider();

    @Before
    public void setUp() {
        initMocks(this);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeStore = new FakeStore(mConfiguration, mFakeThreadUtils, mFakeTaskQueue, mFakeClock);
        mFakeThreadUtils.enforceMainThread(false);
        mFakeTaskQueue.initialize(() -> {});
        mSessionCache = getSessionCache();
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
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
        assertThat(mSessionCache.isHeadInitialized()).isFalse();
        assertThat(mSessionCache.getHead()).isNotNull();
        assertThat(mSessionCache.getHead().isHeadEmpty()).isTrue();
        mSessionCache.initialize();

        // Initialization adds $HEAD
        assertThat(mSessionCache.isHeadInitialized()).isTrue();
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
        assertThat(mSessionCache.getHead()).isNotNull();
        assertThat(mSessionCache.getHead().isHeadEmpty()).isFalse();
        assertThat(mSessionCache.getHead().getSchemaVersion()).isEqualTo(schemaVersion);
    }

    @Test
    public void testInitialization_accessibleContentShouldBeDeterminedAtGcTime() {
        FakeDirectExecutor fakeDirectExecutor = FakeDirectExecutor.queueAllTasks(mFakeThreadUtils);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, fakeDirectExecutor);
        mFakeStore = new FakeStore(mConfiguration, mFakeThreadUtils, mFakeTaskQueue, mFakeClock);
        mFakeThreadUtils.enforceMainThread(false);
        mFakeTaskQueue.initialize(() -> {});
        mSessionCache = getSessionCache();

        Session session = populateSession(1, 2, /* commitToStore= */ true);
        mSessionCache.initialize();
        setSessions(session);
        fakeDirectExecutor.runAllTasks(); // Run GC.

        assertThat(fakeDirectExecutor.hasTasks()).isFalse();
        assertPayloads(/* featureCnt= */ 2);
    }

    @Test
    public void testInitialization_contentGcShouldKeepSharedStates() {
        mFakeStore.setContent("foo",
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

        assertThat(mFakeStore.getSharedStates().getValue()).hasSize(1);
        mSessionCache.initialize();
        assertThat(mFakeStore.getSharedStates().getValue()).hasSize(1);
    }

    @Test
    public void testInitialization_contentGcShouldDiscardSharedStates() {
        mFakeStore.setContent("foo",
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

        assertThat(mFakeStore.getSharedStates().getValue()).hasSize(1);
        mSessionCache.initialize();
        assertThat(mFakeStore.getSharedStates().getValue()).isEmpty();
    }

    @Test
    public void testPutGet() {
        mSessionCache.initialize();
        Session session = populateSession(1, 2);
        mSessionCache.putAttachedAndRetainMetadata(session.getSessionId(), session);

        Session ret = mSessionCache.getAttached(session.getSessionId());
        assertThat(ret).isEqualTo(session);
    }

    @Test
    public void testPut_persisted() {
        mSessionCache.initialize();
        Session session = populateSession(1, 2);
        mSessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        Session ret = mSessionCache.getAttached(session.getSessionId());
        assertThat(ret).isEqualTo(session);

        session = populateSession(2, 2);
        mSessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        List<StreamSession> streamSessionList = mSessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(3);
    }

    @Test
    public void testRemove() {
        mSessionCache.initialize();
        Session session = populateSession(1, 2);
        mSessionCache.putAttached(
                session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);

        List<StreamSession> streamSessionList = mSessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(2);

        String id = session.getSessionId();
        mSessionCache.removeAttached(id);
        assertThat(mSessionCache.getAttached(id)).isNull();

        streamSessionList = mSessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(1);
    }

    @Test
    public void testDetach() {
        mSessionCache.initialize();
        Session s1 = populateSession(1, 2);
        String s1Id = s1.getSessionId();
        mSessionCache.putAttachedAndRetainMetadata(s1Id, s1);

        List<Session> sessions = mSessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(1);
        assertThat(sessions).contains(s1);
        assertThat(s1.getModelProvider()).isNotNull();

        mSessionCache.detachModelProvider(s1Id);
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
        assertThat(s1.getModelProvider()).isNull();
    }

    @Test
    public void testGetAttachedSessions() {
        mSessionCache.initialize();
        Session s1 = populateSession(1, 2);
        mSessionCache.putAttachedAndRetainMetadata(s1.getSessionId(), s1);
        Session s2 = populateSession(2, 2);
        mSessionCache.putAttachedAndRetainMetadata(s2.getSessionId(), s2);

        List<Session> sessions = mSessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(2);
        assertThat(sessions).contains(mSessionCache.getAttached(s1.getSessionId()));
        assertThat(sessions).contains(mSessionCache.getAttached(s2.getSessionId()));
    }

    @Test
    public void testGetAllSessions() {
        mSessionCache.initialize();
        Session headSession = mSessionCache.getHead();

        Session s1 = populateSession(1, 2, /* commitToStore= */ true);
        String s1Id = s1.getSessionId();
        mSessionCache.putAttached(s1Id, 1L, SCHEMA_VERSION, s1);

        assertThat(mSessionCache.getAttachedSessions()).containsExactly(s1);
        assertThat(mSessionCache.getAllSessions()).containsExactly(s1, headSession);

        Session s2 = populateSession(2, 2, /* commitToStore= */ true);
        String s2Id = s2.getSessionId();
        mSessionCache.putAttached(s2Id, 2L, SCHEMA_VERSION, s2);

        assertThat(mSessionCache.getAttachedSessions()).containsExactly(s1, s2);
        assertThat(mSessionCache.getAllSessions()).containsExactly(s1, s2, headSession);

        // Detach the session, which will throw it away from SessionCache.
        mSessionCache.detachModelProvider(s1Id);
        assertThat(mSessionCache.getAttachedSessions()).containsExactly(s2);

        List<Session> allSessions = new ArrayList<>(mSessionCache.getAllSessions());
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
        mSessionCache.initialize();
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();

        mSessionCache.reset();
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
    }

    @Test
    public void testReset_sessions() {
        mSessionCache.initialize();
        int sessionCount = 2;
        for (int i = 0; i < sessionCount; i++) {
            Session session = populateSession(i, 2);
            mSessionCache.putAttachedAndRetainMetadata(session.getSessionId(), session);
        }
        List<Session> sessions = mSessionCache.getAttachedSessions();
        assertThat(sessions).hasSize(sessionCount);

        mSessionCache.reset();
        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
    }

    @Test
    public void testIsSessionAlive() {
        mSessionCache.initialize();
        mFakeClock.set(DEFAULT_LIFETIME_MS + 2);
        assertThat(mSessionCache.isSessionAlive("stream:1", SessionMetadata.getDefaultInstance()))
                .isFalse();
        assertThat(mSessionCache.isSessionAlive(
                           Store.HEAD_SESSION_ID, SessionMetadata.getDefaultInstance()))
                .isTrue();
        assertThat(mSessionCache.isSessionAlive("stream:2",
                           SessionMetadata.newBuilder()
                                   .setCreationTimeMillis(DEFAULT_LIFETIME_MS - 1)
                                   .build()))
                .isTrue();
    }

    @Test
    public void testGetPersistedSessions() {
        StreamSession streamSession = StreamSession.getDefaultInstance();
        mockStreamSessions(StreamSessions.newBuilder().addStreamSession(streamSession).build());

        List<StreamSession> sessionList = mSessionCache.getPersistedSessions();
        assertThat(sessionList).containsExactly(streamSession);
    }

    @Test
    public void testCleanupJournals() {
        String sessionId1 = "stream:1";
        String sessionId2 = "stream:2";
        mFakeStore.setStreamStructures(sessionId1, StreamStructure.getDefaultInstance())
                .setStreamStructures(sessionId2, StreamStructure.getDefaultInstance());

        Session s2 = mock(Session.class);
        when(s2.getSessionId()).thenReturn(sessionId2);
        setSessions(s2);
        mSessionCache.cleanupSessionJournals();
        assertThat(mFakeStore.getAllSessions().getValue()).containsExactly(sessionId2);
        assertThat(mSessionCache.getAttached(sessionId2)).isEqualTo(s2);
    }

    @Test
    public void testInitializePersistedSessions_emptyStreamSessions() {
        mFakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(StreamSessions.getDefaultInstance());
        mSessionCache.initialize();

        assertThat(mSessionCache.getAttachedSessions()).isEmpty();
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

        mFakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(streamSessions);
        mSessionCache.initialize();

        assertThat(mSessionCache.hasSession("stream:1")).isFalse();
        assertThat(mSessionCache.hasSession("stream:2")).isTrue();
        assertThat(mSessionCache.getCreationTimeMillis("stream:2"))
                .isEqualTo(DEFAULT_LIFETIME_MS - 1);
        assertThat(mSessionCache.getHeadLastAddedTimeMillis()).isEqualTo(1);
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

        mFakeClock.set(DEFAULT_LIFETIME_MS + 2);

        mockStreamSessions(streamSessions);
        mSessionCache.initialize();

        assertThat(mSessionCache.hasSession("stream:1")).isFalse();
        assertThat(mSessionCache.hasSession("stream:2")).isTrue();
        assertThat(mSessionCache.getCreationTimeMillis("stream:2"))
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
        mSessionCache.initialize();
        mSessionCache.updateHeadMetadata(currentTime, schemaVersion);

        List<Object> content = mFakeStore.getContentById(SessionCache.STREAM_SESSION_CONTENT_ID);
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
        mSessionCache.initialize();

        // persist HEAD into the store
        mSessionCache.updatePersistedSessionsMetadata();
        List<StreamSession> streamSessionList = mSessionCache.getPersistedSessions();
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

        mSessionCache.updatePersistedSessionsMetadata();
        streamSessionList = mSessionCache.getPersistedSessions();
        assertThat(streamSessionList).hasSize(3);
        assertThat(streamSessionList).contains(session1);
        assertThat(streamSessionList).contains(session2);
    }

    @Test
    public void testErrors_persistedSession() {
        mSessionCache.initialize();
        mSessionCache.updatePersistedSessionsMetadata();

        mFakeStore.setAllowGetPayloads(false);
        assertThat(mSessionCache.getPersistedSessions()).isEmpty();
    }

    private SessionCache getSessionCache() {
        mSessionFactory = new SessionFactory(
                mFakeStore, mFakeTaskQueue, mTimingUtils, mFakeThreadUtils, mConfiguration);
        return new SessionCache(mFakeStore, mFakeTaskQueue, mSessionFactory, 10, mTimingUtils,
                mFakeThreadUtils, mFakeClock);
    }

    private void mockStreamSessions(StreamSessions streamSessions) {
        mFakeStore.setContent(SessionCache.STREAM_SESSION_CONTENT_ID,
                StreamPayload.newBuilder().setStreamSessions(streamSessions).build());
    }

    private Session populateSession(int id, int featureCnt) {
        return populateSession(id, featureCnt, /* commitToStore= */ false);
    }

    private Session populateSession(int id, int featureCnt, boolean commitToStore) {
        InitializableSession session = mSessionFactory.getSession();
        String rootId = mIdGenerators.createRootContentId(1);
        List<StreamStructure> head = new ArrayList<>();
        head.add(StreamStructure.newBuilder()
                         .setOperation(Operation.UPDATE_OR_APPEND)
                         .setContentId(rootId)
                         .build());
        for (int i = 0; i < featureCnt; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            head.add(StreamStructure.newBuilder()
                             .setOperation(Operation.UPDATE_OR_APPEND)
                             .setContentId(contentId)
                             .setParentContentId(rootId)
                             .build());
            if (commitToStore) {
                mFakeStore.setContent(contentId,
                        StreamPayload.newBuilder()
                                .setStreamFeature(StreamFeature.getDefaultInstance())
                                .build());
            }
        }
        session.setSessionId("stream:" + id);
        session.bindModelProvider(mFakeModelProvider, mock(ViewDepthProvider.class));
        session.populateModelProvider(head, true, false, UiContext.getDefaultInstance());

        if (commitToStore) {
            mFakeStore.setStreamStructures(session.getSessionId(), head);
        }

        return session;
    }

    private void populateHead() {
        mFakeStore.setStreamStructures(Store.HEAD_SESSION_ID,
                StreamStructure.newBuilder()
                        .setOperation(Operation.UPDATE_OR_APPEND)
                        .setContentId(mIdGenerators.createRootContentId(1))
                        .build());
    }

    private void setSessions(Session... testSessions) {
        for (Session session : testSessions) {
            mSessionCache.putAttached(
                    session.getSessionId(), /* creationTimeMillis= */ 0L, SCHEMA_VERSION, session);
        }
    }

    private void assertPayloads(int featureCnt) {
        for (int i = 0; i < featureCnt; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            assertThat(mFakeStore.getContentById(contentId)).hasSize(1);
        }
    }
}
