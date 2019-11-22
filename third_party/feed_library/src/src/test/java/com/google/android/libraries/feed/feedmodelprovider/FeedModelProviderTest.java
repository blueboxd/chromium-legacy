// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelMutation;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompleted;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.feedmodelprovider.FeedModelProvider.InitializeModel;
import com.google.android.libraries.feed.feedmodelprovider.FeedModelProvider.TokenMutation;
import com.google.android.libraries.feed.feedmodelprovider.FeedModelProvider.TokenTracking;
import com.google.android.libraries.feed.feedmodelprovider.internal.UpdatableModelChild;
import com.google.android.libraries.feed.feedmodelprovider.internal.UpdatableModelToken;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;

/** Tests of the {@link FeedModelProvider}. */
@RunWith(RobolectricTestRunner.class)
public class FeedModelProviderTest {
    private static final String SESSION_ID = "session";

    private final ContentIdGenerators idGenerators = new ContentIdGenerators();
    private final String rootContentId = idGenerators.createRootContentId(0);
    private final FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();

    @Mock
    private TaskQueue taskQueue;
    @Mock
    private FeedSessionManager feedSessionManager;
    @Mock
    private ThreadUtils threadUtils;
    @Mock
    private Configuration config;

    private ModelChild continuationToken;
    private TimingUtils timingUtils = new TimingUtils();
    private FakeMainThreadRunner fakeMainThreadRunner;

    private List<PayloadWithId> childBindings = new ArrayList<>();

    @Before
    public void setUp() {
        initMocks(this);
        childBindings.clear();
        when(feedSessionManager.getStreamFeatures(any())).thenReturn(Result.success(childBindings));
        fakeMainThreadRunner = FakeMainThreadRunner.runTasksImmediately();
    }

    @Test
    public void testMinimalModelProvider() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        // Add a root to the model provider
        ModelMutation mutator = modelProvider.edit();
        assertThat(mutator).isNotNull();

        StreamFeature rootStreamFeature = getRootFeature();
        mutator.addChild(createStreamStructureAndBinding(rootStreamFeature));
        mutator.commit();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        // Verify that we have a root
        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        assertThat(rootFeature.getStreamFeature()).isEqualTo(rootStreamFeature);

        // Verify that the cursor exists but is at end
        ModelCursor modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();
        assertThat(modelCursor.isAtEnd()).isTrue();
    }

    @Test
    public void testRemoveWithAppend() {
        // Enable synthetic tokens.
        initSyntheticTokenConfig(
                /* initialPageSize= */ 10, /* minPageSize= */ 0, /* pageSize= */ 0);
        FeedModelProvider modelProvider = createFeedModelProvider();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        // Create a root with three children attached.
        modelProvider.edit()
                .addChild(createStreamStructureAndBinding(getRootFeature()))
                .addChild(createStreamStructureAndBinding(createFeature(1, rootContentId)))
                .addChild(createStreamStructureAndBinding(createFeature(2, rootContentId)))
                .addChild(createStreamStructureAndBinding(createFeature(3, rootContentId)))
                .commit();

        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        ModelFeature rootFeature = modelProvider.getRootFeature();
        ModelCursor modelCursor = rootFeature.getCursor();
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(1).payload.getStreamFeature());
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(2).payload.getStreamFeature());
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(3).payload.getStreamFeature());

        // Now remove the second and third children, and add more children.
        modelProvider.edit()
                .removeChild(createRemove(rootContentId, createFeatureContentId(2)))
                .removeChild(createRemove(rootContentId, createFeatureContentId(3)))
                .addChild(createStreamStructureAndBinding(createFeature(4, rootContentId)))
                .addChild(createStreamStructureAndBinding(createFeature(5, rootContentId)))
                .commit();

        // The children should now be #1, #4, and #5; they all should be bound.
        modelCursor = rootFeature.getCursor();
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(1).payload.getStreamFeature());
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(4).payload.getStreamFeature());
        assertThat(modelCursor.getNextItem().getModelFeature().getStreamFeature())
                .isEqualTo(childBindings.get(5).payload.getStreamFeature());
    }

    @Test
    public void testEmptyStream() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();

        modelProvider.edit().commit();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();
    }

    @Test
    public void testCursor() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        int featureCnt = 2;
        createTopLevelFeatures(modelProvider, featureCnt).commit();

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();
        assertThat(modelCursor.isAtEnd()).isFalse();

        int cnt = 0;
        while (modelCursor.getNextItem() != null) {
            cnt++;
        }
        assertThat(cnt).isEqualTo(featureCnt);
        assertThat(modelCursor.isAtEnd()).isTrue();
    }

    @Test
    public void testRemove() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        int featureCnt = 2;
        createTopLevelFeatures(modelProvider, featureCnt)
                .removeChild(createRemove(rootContentId, createFeatureContentId(2)))
                .commit();

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();

        // Verify that one of the features was removed by a the last operation
        int cnt = 0;
        while (modelCursor.getNextItem() != null) {
            cnt++;
        }
        assertThat(cnt).isEqualTo(featureCnt - 1);
    }

    @Test
    public void testMultiLevelCursors() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        int featureCnt = 3;
        ModelMutation mutator = createTopLevelFeatures(modelProvider, featureCnt);
        String featureParent = createFeatureContentId(2);
        for (int i = 0; i < featureCnt; i++) {
            mutator.addChild(createStreamStructureAndBinding(
                    createFeature(i + 1 + featureCnt, featureParent)));
        }
        mutator.commit();

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();
        assertThat(modelCursor.isAtEnd()).isFalse();

        int cnt = 0;
        ModelChild child;
        while ((child = modelCursor.getNextItem()) != null) {
            cnt++;
            if (child.getType() == Type.FEATURE) {
                ModelFeature feature = child.getModelFeature();
                ModelCursor nextCursor = feature.getCursor();
                while (nextCursor.getNextItem() != null) {
                    cnt++;
                }
            }
        }
        assertThat(cnt).isEqualTo(featureCnt + featureCnt);
    }

    @Test
    public void testSharedState() {
        ContentId contentId = ContentId.getDefaultInstance();
        StreamSharedState streamSharedState = StreamSharedState.getDefaultInstance();
        when(feedSessionManager.getSharedState(contentId)).thenReturn(streamSharedState);

        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        assertThat(modelProvider.getSharedState(contentId)).isEqualTo(streamSharedState);
    }

    @Test
    public void testTokenTracking() {
        UpdatableModelToken continuationToken =
                new UpdatableModelToken(StreamToken.getDefaultInstance(), false);
        ArrayList<UpdatableModelChild> location = new ArrayList<>();
        String parentContentId = "parent.content.id";
        TokenTracking tokenTracking =
                new TokenTracking(continuationToken, parentContentId, location);
        assertThat(tokenTracking.tokenChild).isEqualTo(continuationToken);
        assertThat(tokenTracking.parentContentId).isEqualTo(parentContentId);
        assertThat(tokenTracking.location).isEqualTo(location);
    }

    @Test
    public void testHandleToken() {
        StreamToken streamToken = StreamToken.getDefaultInstance();
        ModelToken modelToken = mock(ModelToken.class);
        when(modelToken.getStreamToken()).thenReturn(streamToken);

        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        modelProvider.sessionId = SESSION_ID;
        modelProvider.handleToken(modelToken);
        verify(feedSessionManager).handleToken(SESSION_ID, streamToken);
    }

    @Test
    public void testForceRefresh() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        modelProvider.sessionId = SESSION_ID;
        modelProvider.triggerRefresh(RequestReason.OPEN_WITH_CONTENT);
        assertThat(modelProvider.getDelayedTriggerRefreshForTest()).isFalse();
        verify(feedSessionManager)
                .triggerRefresh(SESSION_ID, RequestReason.OPEN_WITH_CONTENT,
                        UiContext.getDefaultInstance());
    }

    @Test
    public void testForceRefresh_differentReason() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        modelProvider.sessionId = SESSION_ID;
        modelProvider.triggerRefresh(RequestReason.HOST_REQUESTED);
        assertThat(modelProvider.getDelayedTriggerRefreshForTest()).isFalse();
        verify(feedSessionManager)
                .triggerRefresh(
                        SESSION_ID, RequestReason.HOST_REQUESTED, UiContext.getDefaultInstance());
    }

    @Test
    public void testForceRefresh_delayed() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        assertThat(modelProvider.getDelayedTriggerRefreshForTest()).isFalse();
        assertThat(modelProvider.getRequestReasonForTest()).isEqualTo(RequestReason.UNKNOWN);
        modelProvider.triggerRefresh(RequestReason.HOST_REQUESTED);
        assertThat(modelProvider.getDelayedTriggerRefreshForTest()).isTrue();
        assertThat(modelProvider.getRequestReasonForTest()).isEqualTo(RequestReason.HOST_REQUESTED);
        verify(feedSessionManager, never())
                .triggerRefresh(eq(null), anyInt(), any(UiContext.class));
    }

    @Test
    public void testInvalidate() {
        // Create a valid model
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        StreamFeature rootStreamFeature = getRootFeature();
        modelProvider.edit().addChild(createStreamStructureAndBinding(rootStreamFeature)).commit();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();

        modelProvider.invalidate();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
        assertThat(modelCursor.isAtEnd()).isTrue();
    }

    @Test
    public void testInvalidate_unregister() {
        fakeMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        fakeMainThreadRunner.runAllTasks();

        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        modelProvider.invalidate();
        modelProvider.unregisterObserver(observer);
        fakeMainThreadRunner.runAllTasks();
        verify(observer, never()).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void testObserverLifecycle() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelProviderObserver observer1 = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer1);
        verify(observer1, never()).onSessionStart(UiContext.getDefaultInstance());

        StreamFeature rootStreamFeature = getRootFeature();
        modelProvider.edit().addChild(createStreamStructureAndBinding(rootStreamFeature)).commit();
        verify(observer1).onSessionStart(UiContext.getDefaultInstance());

        ModelProviderObserver observer2 = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer2);
        verify(observer2).onSessionStart(UiContext.getDefaultInstance());

        modelProvider.invalidate();
        verify(observer1).onSessionFinished(UiContext.getDefaultInstance());
        verify(observer2).onSessionFinished(UiContext.getDefaultInstance());

        ModelProviderObserver observer3 = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer3);
        verify(observer3).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void testObserverLifecycle_resetRoot() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        verify(observer, never()).onSessionStart(UiContext.getDefaultInstance());

        StreamFeature rootStreamFeature = getRootFeature();
        modelProvider.edit().addChild(createStreamStructureAndBinding(rootStreamFeature)).commit();
        verify(observer).onSessionStart(UiContext.getDefaultInstance());

        String anotherRootId = idGenerators.createRootContentId(100);
        rootStreamFeature = StreamFeature.newBuilder().setContentId(anotherRootId).build();
        modelProvider.edit().addChild(createStreamStructureAndBinding(rootStreamFeature)).commit();
        verify(observer).onSessionFinished(UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
    }

    @Test
    public void testObserverList() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);

        Collection<ModelProviderObserver> observers = modelProvider.getObserversToNotify();
        assertThat(observers.size()).isEqualTo(1);
        assertThat(observers).contains(observer);
    }

    @Test
    public void testRaiseError_noCards() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);

        ModelError modelError = new ModelError(ErrorType.NO_CARDS_ERROR, null);
        modelProvider.raiseError(modelError);
        verify(observer).onError(modelError);
    }

    @Test
    public void testRaiseError_pagination() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();

        int featureCnt = 3;
        StreamToken streamToken = initializeStreamWithToken(modelProvider, featureCnt);
        ModelChild tokenChild = getContinuationToken(modelProvider.getRootFeature());
        assertThat(tokenChild).isNotNull();

        ModelError modelError =
                new ModelError(ErrorType.PAGINATION_ERROR, streamToken.getNextPageToken());
        TokenCompletedObserver observer = mock(TokenCompletedObserver.class);
        tokenChild.getModelToken().registerObserver(observer);
        modelProvider.raiseError(modelError);
        verify(observer).onError(modelError);
    }

    @Test
    public void testInitializationModelMutationHandler() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        InitializeModel initializeModel =
                modelProvider.new InitializeModel(UiContext.getDefaultInstance());
        initializeModel.postMutation();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        verify(observer).onSessionStart(UiContext.getDefaultInstance());
    }

    @Test
    public void testTokens() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();

        int featureCnt = 3;
        StreamToken streamToken = initializeStreamWithToken(modelProvider, featureCnt);

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();
        int cnt = 0;
        continuationToken = null;
        ModelChild child;
        while ((child = modelCursor.getNextItem()) != null) {
            cnt++;
            if (child.getType() == Type.TOKEN) {
                continuationToken = child;
            }
        }
        assertThat(cnt).isEqualTo(featureCnt + 1);
        assertThat(continuationToken).isNotNull();
        assertThat(continuationToken.getModelToken().getStreamToken()).isEqualTo(streamToken);
    }

    @Test
    public void testTokenMutation() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();

        int featureCnt = 3;
        StreamToken streamToken = initializeStreamWithToken(modelProvider, featureCnt);
        Map<ByteString, TokenTracking> tokens = modelProvider.getTokensForTest();
        assertThat(tokens).hasSize(1);

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();
        continuationToken = null;
        ModelChild child;
        while ((child = modelCursor.getNextItem()) != null) {
            if (child.getType() == Type.TOKEN) {
                continuationToken = child;
            }
        }
        assertThat(continuationToken).isNotNull();

        TokenMutation tokenMutation = modelProvider.new TokenMutation(streamToken);
        TokenTracking tokenTracking = tokenMutation.getTokenTrackingForTest();
        assertThat(tokenTracking.location.size()).isEqualTo(featureCnt + 1);

        tokenMutation.preMutation();
        assertThat(tokenTracking.location.size()).isEqualTo(featureCnt + 1);
        assertThat(tokenMutation.newCursorStart).isEqualTo(tokenTracking.location.size() - 1);
        tokens = modelProvider.getTokensForTest();
        assertThat(tokens).hasSize(0);

        TokenCompletedObserver tokenCompletedObserver = new TokenCompletedObserver() {
            @Override
            public void onTokenCompleted(TokenCompleted tokenCompleted) {
                assertThat(tokenCompleted).isNotNull();
            }

            @Override
            public void onError(ModelError error) {}
        };
        continuationToken.getModelToken().registerObserver(tokenCompletedObserver);
        tokenMutation.postMutation();
    }

    @Test
    public void testSyntheticToken() {
        int initialPageSize = 4;
        int pageSize = 4;
        initSyntheticTokenConfig(
                /* initialPageSize= */ initialPageSize, /* minPageSize= */ 2,
                /* pageSize= */ pageSize);

        doAnswer(invocation -> {
            Object[] args = invocation.getArguments();
            ((Runnable) args[2]).run();
            return null;
        })
                .when(taskQueue)
                .execute(anyInt(), anyInt(), any());

        FeedModelProvider modelProvider = createFeedModelProvider();
        int featureCnt = 11;
        createTopLevelFeatures(modelProvider, featureCnt).commit();

        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();

        ModelChild lastChild = assertCursorSize(modelCursor, initialPageSize + 1);
        assertThat(lastChild.getType()).isEqualTo(Type.TOKEN);
        assertThat(lastChild.getModelToken().isSynthetic()).isTrue();

        // The token should be handled in the FeedModelProvider
        modelProvider.handleToken(lastChild.getModelToken());
        modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();

        lastChild = assertCursorSize(modelCursor, initialPageSize + pageSize + 1);
        assertThat(lastChild.getType()).isEqualTo(Type.TOKEN);
        assertThat(lastChild.getModelToken().isSynthetic()).isTrue();

        // last page
        modelProvider.handleToken(lastChild.getModelToken());
        modelCursor = rootFeature.getCursor();
        assertThat(modelCursor).isNotNull();

        lastChild = assertCursorSize(modelCursor, featureCnt);
        assertThat(lastChild.getType()).isEqualTo(Type.FEATURE);
    }

    @Test
    public void testSyntheticToken_missingToken() {
        int initialPageSize = 4;
        int pageSize = 4;
        initSyntheticTokenConfig(
                /* initialPageSize= */ initialPageSize, /* minPageSize= */ 2,
                /* pageSize= */ pageSize);

        doAnswer(invocation -> {
            Object[] args = invocation.getArguments();
            ((Runnable) args[2]).run();
            return null;
        })
                .when(taskQueue)
                .execute(anyInt(), anyInt(), any());

        FeedModelProvider modelProvider = createFeedModelProvider();
        createTopLevelFeatures(modelProvider, /* featureCount= */ 11).commit();

        ModelCursor modelCursor = modelProvider.getRootFeature().getCursor();
        ModelChild lastChild = assertCursorSize(modelCursor, initialPageSize + 1);
        assertThat(lastChild.getType()).isEqualTo(Type.TOKEN);
        assertThat(lastChild.getModelToken().isSynthetic()).isTrue();

        // Simulate removing the synthetic token from the root.
        FakeTokenCompletedObserver observer = new FakeTokenCompletedObserver();
        lastChild.getModelToken().registerObserver(observer);
        modelProvider.clearRootChildrenForTest();
        modelProvider.handleToken(lastChild.getModelToken());
        assertThat(observer.errorThrown).isNotNull();
        assertThat(observer.errorThrown.getErrorType()).isEqualTo(ErrorType.SYNTHETIC_TOKEN_ERROR);
    }

    @Test
    public void testViewDepthProvider_null() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        assertThat(modelProvider.getViewDepthProvider(null)).isNull();
    }

    @Test
    public void testViewDepthProvider_delegatedNull() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ViewDepthProvider mockProvider = mock(ViewDepthProvider.class);
        when(mockProvider.getChildViewDepth()).thenReturn(null);
        assertThat(modelProvider.getViewDepthProvider(null)).isNull();
    }

    @Test
    public void testViewDepthProvider_delegated() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        int featureCnt = 3;
        ModelMutation mutator = createTopLevelFeatures(modelProvider, featureCnt);
        String featureParent = createFeatureContentId(2);
        for (int i = 0; i < featureCnt; i++) {
            mutator.addChild(createStreamStructureAndBinding(
                    createFeature(i + 1 + featureCnt, featureParent)));
        }
        String childFeatureContentId = createFeatureContentId(1 + featureCnt);
        mutator.commit();

        ViewDepthProvider mockProvider = mock(ViewDepthProvider.class);
        when(mockProvider.getChildViewDepth()).thenReturn(childFeatureContentId);

        ViewDepthProvider provider = modelProvider.getViewDepthProvider(mockProvider);
        assertThat(provider.getChildViewDepth()).isEqualTo(featureParent);
    }

    @Test
    public void testGetEmptyRoot() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        ModelMutation mutation = getRootedModelMutator(modelProvider);
        childBindings.clear();
        mutation.commit();

        assertThat(modelProvider.getRootFeature()).isNull();
        assertThat(fakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.ROOT_NOT_BOUND_TO_FEATURE);
    }

    @Test
    public void testGetAllRootChildren() {
        FeedModelProvider modelProvider = createFeedModelProviderWithConfig();
        int featureCnt = 3;
        createTopLevelFeatures(modelProvider, featureCnt).commit();

        List<ModelChild> rootChildren = modelProvider.getAllRootChildren();
        assertThat(rootChildren).hasSize(featureCnt);
    }

    private ModelChild getContinuationToken(ModelFeature rootFeature) {
        ModelCursor modelCursor = rootFeature.getCursor();
        ModelChild token = null;
        ModelChild child;
        while ((child = modelCursor.getNextItem()) != null) {
            if (child.getType() == Type.TOKEN) {
                token = child;
            }
        }
        assertThat(token).isNotNull();
        return token;
    }

    private ModelChild assertCursorSize(ModelCursor cursor, int size) {
        int cnt = 0;
        ModelChild lastChild = null;
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            cnt++;
            lastChild = child;
        }
        assertThat(cnt).isEqualTo(size);
        assertThat(lastChild).isNotNull();
        return lastChild;
    }

    private void initDefaultConfig() {
        initSyntheticTokenConfig(
                /* initialPageSize= */ 0L, /* minPageSize= */ 0L, /* pageSize= */ 0L);
    }

    private void initSyntheticTokenConfig(long initialPageSize, long minPageSize, long pageSize) {
        when(config.getValueOrDefault(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, 0L))
                .thenReturn(initialPageSize);
        when(config.getValueOrDefault(ConfigKey.NON_CACHED_PAGE_SIZE, 0L)).thenReturn(pageSize);
        when(config.getValueOrDefault(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, 0L))
                .thenReturn(minPageSize);
    }

    private StreamToken initializeStreamWithToken(FeedModelProvider modelProvider, int featureCnt) {
        ModelMutation mutator = createTopLevelFeatures(modelProvider, featureCnt);

        // Populate the model provider with a continuation token at the end.
        ByteString bytes = ByteString.copyFrom("continuation", Charset.defaultCharset());
        StreamToken streamToken =
                StreamToken.newBuilder().setNextPageToken(bytes).setParentId(rootContentId).build();
        mutator.addChild(createStreamStructureAndBinding(streamToken));
        mutator.commit();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        return streamToken;
    }

    private FeedModelProvider createFeedModelProviderWithConfig() {
        initDefaultConfig();
        return createFeedModelProvider();
    }

    private FeedModelProvider createFeedModelProvider() {
        return new FeedModelProvider(feedSessionManager, threadUtils, timingUtils, taskQueue,
                fakeMainThreadRunner, null, config, fakeBasicLoggingApi);
    }

    private ModelMutation createTopLevelFeatures(ModelProvider modelProvider, int featureCount) {
        ModelMutation mutator = getRootedModelMutator(modelProvider);
        for (int i = 0; i < featureCount; i++) {
            mutator.addChild(createStreamStructureAndBinding(createFeature(i + 1, rootContentId)));
        }
        return mutator;
    }

    private StreamFeature createFeature(int i, String parentContentId) {
        return StreamFeature.newBuilder()
                .setParentId(parentContentId)
                .setContentId(createFeatureContentId(i))
                .setCard(Card.getDefaultInstance())
                .build();
    }

    private String createFeatureContentId(int i) {
        return idGenerators.createFeatureContentId(i);
    }

    private ModelMutation getRootedModelMutator(ModelProvider modelProvider) {
        ModelMutation mutator = modelProvider.edit();
        assertThat(mutator).isNotNull();
        StreamFeature rootStreamFeature = getRootFeature();
        mutator.addChild(createStreamStructureAndBinding(rootStreamFeature));
        return mutator;
    }

    private StreamStructure createStreamStructureFromFeature(StreamFeature feature) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(feature.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (feature.hasParentId()) {
            builder.setParentContentId(feature.getParentId());
        }
        return builder.build();
    }

    private StreamStructure createStreamStructureFromToken(StreamToken token) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(token.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (token.hasParentId()) {
            builder.setParentContentId(token.getParentId());
        }
        return builder.build();
    }

    private StreamStructure createRemove(String parentContentId, String contentId) {
        return StreamStructure.newBuilder()
                .setContentId(contentId)
                .setParentContentId(parentContentId)
                .setOperation(Operation.REMOVE)
                .build();
    }

    /**
     * This has the side affect of populating {@code childBindings} with a {@code PayloadWithId}.
     */
    private StreamStructure createStreamStructureAndBinding(StreamFeature feature) {
        StreamPayload payload = StreamPayload.newBuilder().setStreamFeature(feature).build();
        childBindings.add(new PayloadWithId(feature.getContentId(), payload));
        return createStreamStructureFromFeature(feature);
    }

    /**
     * This has the side affect of populating {@code childBindings} with a {@code PayloadWithId}.
     */
    private StreamStructure createStreamStructureAndBinding(StreamToken token) {
        StreamPayload payload = StreamPayload.newBuilder().setStreamToken(token).build();
        childBindings.add(new PayloadWithId(token.getContentId(), payload));
        return createStreamStructureFromToken(token);
    }

    private StreamFeature getRootFeature() {
        return StreamFeature.newBuilder().setContentId(rootContentId).build();
    }

    private static class FakeTokenCompletedObserver implements TokenCompletedObserver {
        private ModelError errorThrown;

        @Override
        public void onTokenCompleted(TokenCompleted tokenCompleted) {}

        @Override
        public void onError(ModelError modelError) {
            errorThrown = modelError;
        }
    }
}
