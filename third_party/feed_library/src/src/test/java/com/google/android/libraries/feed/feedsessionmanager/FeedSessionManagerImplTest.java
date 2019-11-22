// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager;

import static com.google.android.libraries.feed.api.internal.store.Store.HEAD_SESSION_ID;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.SessionState;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.intern.Interner;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedAppLifecycleListener;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import com.google.android.libraries.feed.feedmodelprovider.FeedModelProviderFactory;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerImpl.SessionMutationTracker;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerImpl.StreamSharedStateInterner;
import com.google.android.libraries.feed.feedsessionmanager.internal.HeadSessionImpl;
import com.google.android.libraries.feed.feedsessionmanager.internal.Session;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionCache;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.android.libraries.feed.testing.proto.UiContextForTestProto.UiContextForTest;
import com.google.android.libraries.feed.testing.protocoladapter.FakeProtocolAdapter;
import com.google.android.libraries.feed.testing.requestmanager.FakeActionUploadRequestManager;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.android.libraries.feed.testing.store.FakeStore;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.PietSharedStateItemProto.PietSharedStateItem;
import com.google.search.now.wire.feed.ResponseProto.Response;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.List;

/** Tests of the {@link FeedSessionManagerImpl} class. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class FeedSessionManagerImplTest {
    private static final MutationContext EMPTY_MUTATION = new MutationContext.Builder().build();
    private static final ContentId SHARED_STATE_ID = ContentId.newBuilder()
                                                             .setContentDomain("piet-shared-state")
                                                             .setId(1)
                                                             .setTable("piet-shared-state")
                                                             .build();
    private static final String SESSION_ID = "session:1";
    private static final int STORAGE_MISS_THRESHOLD = 4;

    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();
    private final ContentIdGenerators idGenerators = new ContentIdGenerators();
    private final FakeClock fakeClock = new FakeClock();
    private final String rootContentId = idGenerators.createRootContentId(0);
    private final TimingUtils timingUtils = new TimingUtils();

    private Configuration configuration;
    private FakeActionUploadRequestManager fakeActionUploadRequestManager;
    private FakeBasicLoggingApi fakeBasicLoggingApi;
    private FakeMainThreadRunner fakeMainThreadRunner;
    private FakeProtocolAdapter fakeProtocolAdapter;
    private FakeFeedRequestManager fakeRequestManager;
    private FakeStore fakeStore;
    private FakeTaskQueue fakeTaskQueue;
    private FakeThreadUtils fakeThreadUtils;
    private FeedAppLifecycleListener appLifecycleListener;
    @Mock
    private SchedulerApi schedulerApi;

    @Parameters
    public static List<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean uploadingActionsEnabled;

    @Before
    public void setUp() {
        initMocks(this);
        configuration = new Configuration.Builder()
                                .put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, uploadingActionsEnabled)
                                .put(ConfigKey.STORAGE_MISS_THRESHOLD, STORAGE_MISS_THRESHOLD)
                                .build();
        fakeBasicLoggingApi = new FakeBasicLoggingApi();
        fakeThreadUtils = FakeThreadUtils.withThreadChecks();
        fakeMainThreadRunner =
                FakeMainThreadRunner.runTasksImmediatelyWithThreadChecks(fakeThreadUtils);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        appLifecycleListener = new FeedAppLifecycleListener(fakeThreadUtils);
        fakeActionUploadRequestManager = new FakeActionUploadRequestManager(fakeThreadUtils);
        fakeStore = new FakeStore(configuration, fakeThreadUtils, fakeTaskQueue, fakeClock);
        fakeProtocolAdapter = new FakeProtocolAdapter();
        fakeRequestManager = new FakeFeedRequestManager(
                fakeThreadUtils, fakeMainThreadRunner, fakeProtocolAdapter, fakeTaskQueue);
        fakeRequestManager.queueResponse(Response.getDefaultInstance());
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.NO_REQUEST_WITH_CONTENT);
    }

    @Test
    public void testInitialization() {
        StreamSharedState sharedState =
                StreamSharedState.newBuilder()
                        .setContentId(idGenerators.createFeatureContentId(0))
                        .setPietSharedStateItem(PietSharedStateItem.getDefaultInstance())
                        .build();
        StreamStructure operation =
                StreamStructure.newBuilder()
                        .setContentId(idGenerators.createFeatureContentId(0))
                        .setOperation(StreamStructure.Operation.UPDATE_OR_APPEND)
                        .build();
        fakeStore.setSharedStates(sharedState).setStreamStructures(HEAD_SESSION_ID, operation);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(configuration);
        assertThat(sessionManager.initialized.get()).isFalse();
        sessionManager.initialize();
        assertThat(sessionManager.initialized.get()).isTrue();
        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(1);

        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        Session head = sessionCache.getHead();
        assertThat(head).isInstanceOf(HeadSessionImpl.class);
        String itemKey = idGenerators.createFeatureContentId(0);
        assertThat(head.getContentInSession()).containsExactly(itemKey);
    }

    // This is testing a condition similar to the one that caused [INTERNAL LINK].
    @Test
    public void testInitialization_equalSharedStatesDifferentContentIds() throws Exception {
        StreamSharedState sharedState1 =
                StreamSharedState.newBuilder()
                        .setContentId("shared-state-1")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addStylesheets(
                                        Stylesheet.newBuilder().setStylesheetId(
                                                "shared-stylesheet"))))
                        .build();
        StreamSharedState sharedState2 =
                StreamSharedState.newBuilder()
                        .setContentId("shared-state-2") //  Different ContentId
                        .setPietSharedStateItem( // Equal PietSharedStateItem
                                PietSharedStateItem.parseFrom(
                                        sharedState1.getPietSharedStateItem().toByteString()))
                        .build();
        assertThat(sharedState1).isNotEqualTo(sharedState2);

        // Initial PietSharedStateItem messages are equal but not the same between the 2 shared
        // states.
        assertThat(sharedState1.getPietSharedStateItem())
                .isEqualTo(sharedState2.getPietSharedStateItem());
        assertThat(sharedState1.getPietSharedStateItem())
                .isNotSameInstanceAs(sharedState2.getPietSharedStateItem());

        StreamStructure operation =
                StreamStructure.newBuilder()
                        .setContentId(idGenerators.createFeatureContentId(0))
                        .setOperation(StreamStructure.Operation.UPDATE_OR_APPEND)
                        .build();
        fakeStore.setSharedStates(sharedState1, sharedState2)
                .setStreamStructures(HEAD_SESSION_ID, operation);

        ContentId contentId1 = SHARED_STATE_ID.toBuilder().setId(1).build();
        ContentId contentId2 = SHARED_STATE_ID.toBuilder().setId(2).build();
        fakeProtocolAdapter.addContentId("shared-state-1", contentId1)
                .addContentId("shared-state-2", contentId2);

        FeedSessionManagerImpl sessionManager = createFeedSessionManager(configuration);
        assertThat(sessionManager.initialized.get()).isFalse();
        sessionManager.initialize();
        assertThat(sessionManager.initialized.get()).isTrue();
        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(2);

        StreamSharedState cachedSharedState1 = sessionManager.getSharedState(contentId1);
        StreamSharedState cachedSharedState2 = sessionManager.getSharedState(contentId2);
        assertThat(cachedSharedState1).isEqualTo(sharedState1);
        assertThat(cachedSharedState2).isEqualTo(sharedState2);

        // Cached PietSharedStateItem messages the same between the 2 shared states (memoized).
        assertThat(cachedSharedState1.getPietSharedStateItem())
                .isSameInstanceAs(cachedSharedState2.getPietSharedStateItem());
    }

    @Test
    public void testLifecycleInitialization() {
        FeedSessionManagerImpl sessionManager = createFeedSessionManager(configuration);
        assertThat(sessionManager.initialized.get()).isFalse();
        sessionManager.onLifecycleEvent(LifecycleEvent.INITIALIZE);
        assertThat(sessionManager.initialized.get()).isTrue();
        sessionManager.onLifecycleEvent(LifecycleEvent.INITIALIZE);
        assertThat(sessionManager.initialized.get()).isTrue();
    }

    @Test
    public void testSessionWithContent() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        int featureCnt = 3;
        populateSession(sessionManager, featureCnt, 1, true, null);

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getRootFeature()).isNotNull();

        ModelCursor cursor = modelProvider.getRootFeature().getCursor();
        int cursorCount = 0;
        while (cursor.getNextItem() != null) {
            cursorCount++;
        }
        assertThat(cursorCount).isEqualTo(featureCnt);

        // append a couple of others
        populateSession(sessionManager, featureCnt, featureCnt + 1, false, null);

        cursor = modelProvider.getRootFeature().getCursor();
        cursorCount = 0;
        while (cursor.getNextItem() != null) {
            cursorCount++;
        }
        assertThat(cursorCount).isEqualTo(featureCnt * 2);
    }

    @Test
    public void testNoRequestWithContent_populateIsImmediate() {
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.NO_REQUEST_WITH_CONTENT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        fakeTaskQueue.resetCounts();

        // Population will happen in an immediate task and no request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(fakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.getUserFacingTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.isMakingRequest()).isFalse();
    }

    @Test
    public void testRequestWithContent_populateIsImmediate() {
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        fakeTaskQueue.resetCounts();

        // Population will happen immediately and a request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(fakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.getUserFacingTaskCount()).isEqualTo(1);
        assertThat(fakeTaskQueue.isMakingRequest()).isTrue();
    }

    @Test
    public void testRequestWithWait_populateIsUserFacing() {
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_WAIT);

        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, 3, 1, true, null);
        fakeTaskQueue.resetCounts();

        // Population will happen in a user-facing task and a request is sent.
        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(fakeTaskQueue.getImmediateTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.getUserFacingTaskCount()).isEqualTo(2);
        assertThat(fakeTaskQueue.isMakingRequest()).isTrue();
    }

    @Test
    public void testGetExistingSession_populateIsImmediate() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager,
                /* featureCnt= */ 2,
                /* idStart= */ 1,
                /* reset= */ true,
                /* sharedStateId= */ null);
        ModelProvider modelProvider = getModelProvider(sessionManager);
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();
        fakeTaskQueue.resetCounts();

        // Population will happen in an immediate task.
        modelProvider = getModelProvider(sessionManager, sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(fakeTaskQueue.getImmediateTaskCount()).isEqualTo(1);
        assertThat(fakeTaskQueue.getBackgroundTaskCount()).isEqualTo(0);
        assertThat(fakeTaskQueue.getUserFacingTaskCount()).isEqualTo(0);
    }

    @Test
    public void testMissingFeaturesBeyondThreshold_switchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, STORAGE_MISS_THRESHOLD, 1, true, null);
        fakeStore.clearContent();

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(fakeStore.isEphemeralMode()).isTrue();
        assertThat(fakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.STORAGE_MISS_BEYOND_THRESHOLD);
    }

    @Test
    public void testMissingFeaturesAtThreshold_doesNotSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager, STORAGE_MISS_THRESHOLD - 1, 1, true, null);
        fakeStore.clearContent();

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider).isNotNull();
        assertThat(fakeStore.isEphemeralMode()).isFalse();
        assertThat(fakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.CONTENT_STORAGE_MISSING_ITEM);
    }

    @Test
    public void testNoCardsError() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.getUpdateConsumer(EMPTY_MUTATION).accept(Result.failure());

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider.getRootFeature()).isNull();

        // Verify the failed session is correct
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).hasSize(1);
        Session session = sessionCache.getAttached(modelProvider.getSessionId());
        assertThat(session).isNotNull();
    }

    @Test
    public void testNoCardsError_populatedHeadSuppressesError() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        populateSession(sessionManager,
                /* featureCnt= */ 2,
                /* idStart= */ 1,
                /* reset= */ true,
                /* sharedStateId= */ null);
        sessionManager.getUpdateConsumer(EMPTY_MUTATION).accept(Result.failure());

        ModelProvider modelProvider = getModelProvider(sessionManager);
        assertThat(modelProvider.getRootFeature()).isNotNull();

        // Verify the failed session is correct
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).hasSize(1);
        Session session = sessionCache.getAttached(modelProvider.getSessionId());
        assertThat(session).isNotNull();
    }

    @Test
    public void testModelErrorObserver() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        // verify this runs.  Another method that can'be be verified on a single thread since
        // the noCardsError will be set and unset.
        sessionManager.modelErrorObserver(null, new ModelError(ErrorType.NO_CARDS_ERROR, null));
    }

    @Test
    public void testReset() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        int featureCnt = 3;
        int fullFeatureCount = populateSession(sessionManager, featureCnt, 1, true, null);
        assertThat(fullFeatureCount).isEqualTo(featureCnt + 1);

        fullFeatureCount = populateSession(sessionManager, featureCnt, 1, true, null);
        assertThat(fullFeatureCount).isEqualTo(featureCnt + 1);
    }

    @Test
    public void testHandleToken() {
        ByteString bytes = ByteString.copyFrom("continuation", Charset.defaultCharset());
        StreamToken streamToken =
                StreamToken.newBuilder().setNextPageToken(bytes).setParentId(rootContentId).build();
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.handleToken(SESSION_ID, streamToken);

        assertThat(fakeRequestManager.getLatestStreamToken()).isEqualTo(streamToken);
        assertThat(fakeStore.getContentById(SessionCache.CONSISTENCY_TOKEN_CONTENT_ID))
                .hasSize(uploadingActionsEnabled ? 1 : 0);
    }

    @Test
    public void testForceRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.triggerRefresh(
                SESSION_ID, RequestReason.ZERO_STATE, UiContext.getDefaultInstance());

        assertThat(fakeRequestManager.getLatestRequestReason()).isEqualTo(RequestReason.ZERO_STATE);
    }

    @Test
    public void testForceRefresh_scheduledRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        sessionManager.triggerRefresh(
                SESSION_ID, RequestReason.HOST_REQUESTED, UiContext.getDefaultInstance());

        assertThat(fakeRequestManager.getLatestRequestReason())
                .isEqualTo(RequestReason.HOST_REQUESTED);
    }

    @Test
    public void testGetSharedState() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        String sharedStateId = idGenerators.createSharedStateContentId(0);
        ContentId undefinedSharedStateId = ContentId.newBuilder()
                                                   .setContentDomain("shared-state")
                                                   .setId(5)
                                                   .setTable("shared-states")
                                                   .build();
        String undefinedStreamSharedStateId =
                idGenerators.createSharedStateContentId(undefinedSharedStateId.getId());
        fakeProtocolAdapter.addContentId(sharedStateId, SHARED_STATE_ID)
                .addContentId(undefinedStreamSharedStateId, undefinedSharedStateId);

        populateSession(sessionManager, 3, 1, true, sharedStateId);
        assertThat(sessionManager.getSharedState(SHARED_STATE_ID)).isNotNull();

        // test the null condition
        assertThat(sessionManager.getSharedState(undefinedSharedStateId)).isNull();
    }

    @Test
    public void testUpdateConsumer() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.outstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(updateConsumer).isInstanceOf(SessionMutationTracker.class);
        assertThat(sessionManager.outstandingMutations).hasSize(1);
        assertThat(sessionManager.outstandingMutations).contains(updateConsumer);
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.outstandingMutations).isEmpty();
    }

    @Test
    public void testUpdateConsumer_clearAll() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.outstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(sessionManager.outstandingMutations).hasSize(1);
        appLifecycleListener.onClearAll();
        assertThat(sessionManager.outstandingMutations).isEmpty();

        // verify this still runs (as a noop)
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.outstandingMutations).isEmpty();
    }

    @Test
    public void testUpdateConsumer_clearAllWithRefresh() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        assertThat(sessionManager.outstandingMutations).isEmpty();
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        assertThat(sessionManager.outstandingMutations).hasSize(1);
        appLifecycleListener.onClearAllWithRefresh();
        assertThat(sessionManager.outstandingMutations).isEmpty();

        // verify this still runs (as a noop)
        updateConsumer.accept(Result.success(Model.empty()));
        assertThat(sessionManager.outstandingMutations).isEmpty();
    }

    @Test
    public void testEdit_semanticProperties() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();

        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");
        StreamDataOperation streamDataOperation =
                StreamDataOperation.newBuilder()
                        .setStreamPayload(StreamPayload.newBuilder().setSemanticData(semanticData))
                        .setStreamStructure(StreamStructure.newBuilder()
                                                    .setContentId(rootContentId)
                                                    .setOperation(Operation.UPDATE_OR_APPEND))
                        .build();

        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        Result<Model> result = Result.success(Model.of(ImmutableList.of(streamDataOperation)));
        updateConsumer.accept(result);

        assertThat(fakeStore.getContentById(rootContentId))
                .contains(new SemanticPropertiesWithId(rootContentId, semanticData.toByteArray()));
    }

    @Test
    public void testSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        fakeThreadUtils.enforceMainThread(false);
        sessionManager.switchToEphemeralMode("An Error Message");
        assertThat(fakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testOnSwitchToEphemeralMode() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        String sharedStateId = idGenerators.createSharedStateContentId(0);

        int featureCount = 3;
        populateSession(sessionManager, featureCount, 1, true, sharedStateId);

        assertThat(sessionManager.getSharedStateCacheForTest()).hasSize(1);
        SessionCache sessionCache = sessionManager.getSessionCacheForTest();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        Session session = sessionCache.getHead();
        assertThat(session).isNotNull();
        assertThat(session.getContentInSession()).hasSize(featureCount + 1);

        fakeThreadUtils.enforceMainThread(false);
        sessionManager.onSwitchToEphemeralMode();

        assertThat(sessionManager.getSharedStateCacheForTest()).isEmpty();
        assertThat(sessionCache.getAttachedSessions()).isEmpty();
        session = sessionCache.getHead();
        assertThat(session).isNotNull();
        assertThat(session.getContentInSession()).isEmpty();
    }

    @Test
    public void testErrors_initializationSharedStateError() {
        fakeStore.setAllowGetSharedStates(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        assertThat(fakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_initializationStreamStructureError() {
        fakeStore.setAllowGetStreamStructures(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        assertThat(fakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_createNewSessionError() {
        fakeStore.setAllowCreateNewSession(false);
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        populateSession(sessionManager, 5, 1, true, null);

        ModelProvider unused = getModelProvider(sessionManager);
        assertThat(fakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testErrors_getStreamStructuresError() {
        FeedSessionManagerImpl sessionManager = getUninitializedSessionManager();
        sessionManager.initialize();
        fakeStore.setAllowGetStreamStructures(false);
        populateSession(sessionManager, 5, 1, true, null);

        ModelProvider unused = getModelProvider(sessionManager);
        assertThat(fakeStore.isEphemeralMode()).isTrue();
    }

    @Test
    public void testTriggerUploadActions() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager(
                new Configuration.Builder().put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, true).build());
        ImmutableSet<StreamUploadableAction> actionSet =
                ImmutableSet.of(StreamUploadableAction.getDefaultInstance());
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        fakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        sessionManager.triggerUploadActions(actionSet);
        assertThat(fakeActionUploadRequestManager.getLatestActions())
                .containsExactlyElementsIn(actionSet);
    }

    @Test
    public void testGetConsistencyToken() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager(
                new Configuration.Builder().put(ConfigKey.UNDOABLE_ACTIONS_ENABLED, true).build());
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        fakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        assertThat(sessionManager.getConsistencyToken()).isEqualTo(token);
    }

    @Test
    public void testGetConsistencyTokenEmpty() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        fakeThreadUtils.enforceMainThread(false);
        assertThat(sessionManager.getConsistencyToken())
                .isEqualTo(ConsistencyToken.getDefaultInstance());
    }

    @Test
    public void testFetchActionsAndUpload() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();
        ConsistencyToken token = ConsistencyToken.newBuilder()
                                         .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                         .build();
        ConsistencyToken expectedToken =
                uploadingActionsEnabled ? token : ConsistencyToken.getDefaultInstance();
        Consumer<Result<ConsistencyToken>> consumer = result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue()).isEqualTo(expectedToken);
        };
        fakeActionUploadRequestManager.setResult(Result.success(token));
        fakeThreadUtils.enforceMainThread(false);
        sessionManager.getConsistencyTokenConsumer().accept(Result.success(token));
        sessionManager.fetchActionsAndUpload(consumer);
        assertThat(fakeActionUploadRequestManager.getLatestActions()).isNotNull();
    }

    @Test
    public void testStreamSharedStateInterner() {
        Interner<StreamSharedState> interner = new StreamSharedStateInterner();
        StreamSharedState first =
                StreamSharedState.newBuilder()
                        .setContentId("foo")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("equal"))))
                        .build();
        StreamSharedState second =
                StreamSharedState.newBuilder()
                        .setContentId("baz")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("equal"))))
                        .build();
        StreamSharedState third =
                StreamSharedState.newBuilder()
                        .setContentId("bar")
                        .setPietSharedStateItem(PietSharedStateItem.newBuilder().setPietSharedState(
                                PietSharedState.newBuilder().addTemplates(
                                        Template.newBuilder().setTemplateId("different"))))
                        .build();
        assertThat(first).isNotSameInstanceAs(second);
        assertThat(first.getPietSharedStateItem()).isEqualTo(second.getPietSharedStateItem());
        assertThat(first).isNotEqualTo(third);
        assertThat(first.getPietSharedStateItem()).isNotEqualTo(third.getPietSharedStateItem());

        // Pool is empty so first is added/returned.
        StreamSharedState internedFirst = interner.intern(first);
        assertThat(interner.size()).isEqualTo(1);
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has an identical inner PietSharedStateItem proto, which is used.
        StreamSharedState internedSecond = interner.intern(second);
        assertThat(interner.size()).isEqualTo(1);
        // The returned proto is equal to second, but its internal PietSharedStateItem is the same
        // as the one in first (memoized).
        assertThat(internedSecond).isNotSameInstanceAs(second);
        assertThat(internedSecond).isEqualTo(second);
        assertThat(internedSecond.getPietSharedStateItem())
                .isSameInstanceAs(first.getPietSharedStateItem());

        // Third has a new PietSharedStateItem (not equal with any previous) so it is added to the
        // pool.
        StreamSharedState internedThird = interner.intern(third);
        assertThat(interner.size()).isEqualTo(2);
        assertThat(internedThird).isSameInstanceAs(third);
    }

    @Test
    public void testGetNewSession() {
        FeedSessionManagerImpl sessionManager = getInitializedSessionManager();

        UiContext uiContext = UiContext.newBuilder()
                                      .setExtension(UiContextForTest.uiContextForTest,
                                              UiContextForTest.newBuilder().setValue(3).build())
                                      .build();

        ModelProvider modelProvider = getModelProvider(sessionManager, uiContext);

        sessionManager.getNewSession(modelProvider, /* viewDepthProvider= */ null, uiContext);
        ModelProviderObserver modelProviderObserver = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(modelProviderObserver);

        verify(modelProviderObserver).onSessionStart(uiContext);
    }

    private int populateSession(FeedSessionManagerImpl sessionManager, int featureCnt, int idStart,
            boolean reset,
            /*@Nullable*/ String sharedStateId) {
        int operationCount = 0;

        InternalProtocolBuilder internalProtocolBuilder = new InternalProtocolBuilder();
        if (reset) {
            internalProtocolBuilder.addClearOperation().addRootFeature();
            operationCount++;
        }
        for (int i = 0; i < featureCnt; i++) {
            internalProtocolBuilder.addFeature(
                    contentIdGenerators.createFeatureContentId(idStart++),
                    idGenerators.createRootContentId(0));
            operationCount++;
        }
        if (sharedStateId != null) {
            internalProtocolBuilder.addSharedState(sharedStateId);
            operationCount++;
        }
        Consumer<Result<Model>> updateConsumer = sessionManager.getUpdateConsumer(EMPTY_MUTATION);
        updateConsumer.accept(Result.success(Model.of(internalProtocolBuilder.build())));
        return operationCount;
    }

    private ModelProvider getModelProvider(FeedSessionManager sessionManager) {
        return getModelProvider(
                sessionManager, /* sessionId= */ null, UiContext.getDefaultInstance());
    }

    private ModelProvider getModelProvider(FeedSessionManager sessionManager, UiContext uiContext) {
        return getModelProvider(sessionManager, /* sessionId= */ null, uiContext);
    }

    private ModelProvider getModelProvider(
            FeedSessionManager sessionManager, String sessionId, UiContext uiContext) {
        ModelProviderFactory modelProviderFactory =
                new FeedModelProviderFactory(sessionManager, fakeThreadUtils, timingUtils,
                        fakeTaskQueue, fakeMainThreadRunner, configuration, fakeBasicLoggingApi);
        if (sessionId == null) {
            return modelProviderFactory.createNew(/* viewDepthProvider= */ null, uiContext);
        } else {
            return modelProviderFactory.create(sessionId, uiContext);
        }
    }

    private FeedSessionManagerImpl getInitializedSessionManager() {
        return getInitializedSessionManager(configuration);
    }

    private FeedSessionManagerImpl getInitializedSessionManager(Configuration config) {
        FeedSessionManagerImpl fsm = createFeedSessionManager(config);
        fsm.initialize();
        return fsm;
    }

    private FeedSessionManagerImpl getUninitializedSessionManager() {
        return createFeedSessionManager(configuration);
    }

    private FeedSessionManagerImpl createFeedSessionManager(Configuration configuration) {
        return new FeedSessionManagerFactory(fakeTaskQueue, fakeStore, timingUtils, fakeThreadUtils,
                fakeProtocolAdapter, fakeRequestManager, fakeActionUploadRequestManager,
                schedulerApi, configuration, fakeClock, appLifecycleListener, fakeMainThreadRunner,
                fakeBasicLoggingApi)
                .create();
    }
}
