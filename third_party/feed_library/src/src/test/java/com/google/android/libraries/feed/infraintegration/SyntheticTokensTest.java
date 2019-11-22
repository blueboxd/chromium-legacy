// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.android.libraries.feed.common.testing.ResponseBuilder.ROOT_CONTENT_ID;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompleted;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.PagingState;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;

/** Test Synthetic tokens. */
@RunWith(RobolectricTestRunner.class)
public class SyntheticTokensTest {
    private static final int INITIAL_PAGE_SIZE = 4;
    private static final int PAGE_SIZE = 4;
    private static final int MIN_PAGE_SIZE = 2;

    private FakeClock fakeClock;
    private FakeFeedRequestManager fakeFeedRequestManager;
    private FeedSessionManager feedSessionManager;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private ProtocolAdapter protocolAdapter;
    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();

    @Before
    public void setUp() {
        initMocks(this);
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, (long) INITIAL_PAGE_SIZE)
                        .put(ConfigKey.NON_CACHED_PAGE_SIZE, (long) PAGE_SIZE)
                        .put(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, (long) MIN_PAGE_SIZE)
                        .build();
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder().setConfiguration(configuration).build();
        fakeClock = scope.getFakeClock();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        feedSessionManager = scope.getFeedSessionManager();
        modelProviderFactory = scope.getModelProviderFactory();
        protocolAdapter = scope.getProtocolAdapter();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    /**
     * This test will test the creation of synthetic tokens.
     *
     * <ol>
     *   <li>Create an initial $HEAD with 13 items
     *   <li>Clear the FeedSessionManager ContentCache to simulate non-cached mode
     *   <li>Create a new session which will have a synthetic token at INITIAL_PAGE_SIZE
     *   <li>FeedSessionManager.handleToken on the synthetic token, verify full cursor and partial
     *       page cursor
     *   <li>FeedSessionManager.handleToken on the next synthetic token, verify we get PAGE_SIZE +
     *       slop. Verify both the full cursor and the partial page cursor. No further tokens will
     * be in the cursor.
     * </ol>
     */
    @Test
    public void syntheticTokenPaging() {
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
                ResponseBuilder.createFeatureContentId(5),
                ResponseBuilder.createFeatureContentId(6),
                ResponseBuilder.createFeatureContentId(7),
                ResponseBuilder.createFeatureContentId(9),
                ResponseBuilder.createFeatureContentId(10),
                ResponseBuilder.createFeatureContentId(11),
                ResponseBuilder.createFeatureContentId(12),
                ResponseBuilder.createFeatureContentId(13)};

        // Create 13 cards (initial page size + page size + page size and slope of 1)
        // Initial model will have all the cards
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        ModelChild token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOf(cards, INITIAL_PAGE_SIZE));
        assertThat(token.getModelToken().isSynthetic()).isTrue();

        // clear the ContentCache
        clearSessionManagerContentCache();

        // Create a new ModelProvider and verify the first page size is INITIAL_PAGE_SIZE (4)
        modelProvider = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        cursor = root.getCursor();
        token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOf(cards, INITIAL_PAGE_SIZE));

        // Register observer, handle token, verify the full cursor and the observer's cursor
        // This should be the second page (PAGE_SIZE) and there will still be a new synthetic token
        TokenCompletedObserver tokenCompletedObserver = mock(TokenCompletedObserver.class);
        token.getModelToken().registerObserver(tokenCompletedObserver);
        modelProvider.handleToken(token.getModelToken());
        ArgumentCaptor<TokenCompleted> completedArgumentCaptor =
                ArgumentCaptor.forClass(TokenCompleted.class);
        verify(tokenCompletedObserver).onTokenCompleted(completedArgumentCaptor.capture());
        ModelCursor pageCursor = completedArgumentCaptor.getValue().getCursor();
        assertThat(pageCursor).isNotNull();
        modelValidator.assertCursorContentsWithToken(cursor,
                Arrays.copyOfRange(cards, INITIAL_PAGE_SIZE, INITIAL_PAGE_SIZE + PAGE_SIZE));
        cursor = root.getCursor();
        token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOf(cards, INITIAL_PAGE_SIZE + PAGE_SIZE));

        // Register observer, handle token, verify that we pick up PAGE_SIZE + slop, verify
        // observer cursor and full cursor, no further token will be in the cursor.
        tokenCompletedObserver = mock(TokenCompletedObserver.class);
        token.getModelToken().registerObserver(tokenCompletedObserver);
        modelProvider.handleToken(token.getModelToken());
        completedArgumentCaptor = ArgumentCaptor.forClass(TokenCompleted.class);
        verify(tokenCompletedObserver).onTokenCompleted(completedArgumentCaptor.capture());
        pageCursor = completedArgumentCaptor.getValue().getCursor();
        assertThat(pageCursor).isNotNull();
        modelValidator.assertCursorContents(
                pageCursor, Arrays.copyOfRange(cards, INITIAL_PAGE_SIZE + PAGE_SIZE, cards.length));
        modelProvider.handleToken(token.getModelToken());
        cursor = root.getCursor();
        modelValidator.assertCursorContents(cursor, cards);
    }

    /**
     * This test will test the creation of synthetic tokens.
     *
     * <ol>
     *   <li>Create an initial $HEAD with 13 items
     *   <li>Clear the FeedSessionManager ContentCache to simulate non-cached mode
     *   <li>Create a new session which will have a synthetic token at INITIAL_PAGE_SIZE
     *   <li>FeedSessionManager.handleToken on the synthetic token, verify full cursor and partial
     *       page cursor
     *   <li>FeedSessionManager.handleToken on the next synthetic token, verify we get PAGE_SIZE +
     *       slop. Verify both the full cursor and the partial page cursor. No further tokens will
     * be in the cursor.
     * </ol>
     */
    @Test
    public void syntheticTokenPaging_withCache() {
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
                ResponseBuilder.createFeatureContentId(5),
                ResponseBuilder.createFeatureContentId(6),
                ResponseBuilder.createFeatureContentId(7),
                ResponseBuilder.createFeatureContentId(9),
                ResponseBuilder.createFeatureContentId(10),
                ResponseBuilder.createFeatureContentId(11),
                ResponseBuilder.createFeatureContentId(12),
                ResponseBuilder.createFeatureContentId(13)};

        // Create 13 cards (initial page size + page size + page size and slope of 1)
        // Initial model will have all the cards
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        ModelChild token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOfRange(cards, 0, INITIAL_PAGE_SIZE));
        assertThat(token.getModelToken().isSynthetic()).isTrue();

        // Create a new ModelProvider and verify the first page size is INITIAL_PAGE_SIZE (4)
        modelProvider = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        cursor = root.getCursor();
        token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOf(cards, INITIAL_PAGE_SIZE));

        // Register observer, handle token, verify the full cursor and the observer's cursor
        // This should be the second page (PAGE_SIZE) and there will still be a new synthetic token
        TokenCompletedObserver tokenCompletedObserver = mock(TokenCompletedObserver.class);
        token.getModelToken().registerObserver(tokenCompletedObserver);
        modelProvider.handleToken(token.getModelToken());
        ArgumentCaptor<TokenCompleted> completedArgumentCaptor =
                ArgumentCaptor.forClass(TokenCompleted.class);
        verify(tokenCompletedObserver).onTokenCompleted(completedArgumentCaptor.capture());
        ModelCursor pageCursor = completedArgumentCaptor.getValue().getCursor();
        assertThat(pageCursor).isNotNull();
        modelValidator.assertCursorContentsWithToken(cursor,
                Arrays.copyOfRange(cards, INITIAL_PAGE_SIZE, INITIAL_PAGE_SIZE + PAGE_SIZE));
        cursor = root.getCursor();
        token = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOfRange(cards, 0, INITIAL_PAGE_SIZE + PAGE_SIZE));

        // Register observer, handle token, verify that we pick up PAGE_SIZE + slop, verify
        // observer cursor and full cursor, no further token will be in the cursor.
        tokenCompletedObserver = mock(TokenCompletedObserver.class);
        token.getModelToken().registerObserver(tokenCompletedObserver);
        modelProvider.handleToken(token.getModelToken());
        completedArgumentCaptor = ArgumentCaptor.forClass(TokenCompleted.class);
        verify(tokenCompletedObserver).onTokenCompleted(completedArgumentCaptor.capture());
        pageCursor = completedArgumentCaptor.getValue().getCursor();
        assertThat(pageCursor).isNotNull();
        modelValidator.assertCursorContents(
                pageCursor, Arrays.copyOfRange(cards, INITIAL_PAGE_SIZE + PAGE_SIZE, cards.length));
        modelProvider.handleToken(token.getModelToken());
        cursor = root.getCursor();
        modelValidator.assertCursorContents(cursor, cards);
    }

    /**
     * Test Synthetic tokens on paged results.
     *
     * <ol>
     *   <li>Create an initial $HEAD with 4 items and a second page of 8 additional items
     *   <li>Create a new session and validate the expected cards and a real next page token.
     *   <li>Handle the token to bring in the second page of items
     *   <li>Validate that we have 8 total items, plus a synthetic token
     *   <li>Handle the synthetic token
     *   <li>Validate that we have all 12 total items with no token
     * </ol>
     */
    @Test
    public void syntheticTokenPaging_paging() {
        ContentId[] cards = new ContentId[] {
                ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
        };
        ContentId[] pageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(5),
                ResponseBuilder.createFeatureContentId(6),
                ResponseBuilder.createFeatureContentId(7),
                ResponseBuilder.createFeatureContentId(8),
                ResponseBuilder.createFeatureContentId(9),
                ResponseBuilder.createFeatureContentId(10),
                ResponseBuilder.createFeatureContentId(11),
                ResponseBuilder.createFeatureContentId(12)};

        PagingState state = new PagingState(cards, pageCards, 1, contentIdGenerators);
        fakeFeedRequestManager.queueResponse(state.initialResponse);
        fakeFeedRequestManager.queueResponse(state.pageResponse);
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        ModelChild tokenFeature = modelValidator.assertCursorContentsWithToken(cursor, cards);
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isFalse();

        ModelToken modelToken = tokenFeature.getModelToken();
        modelProvider.handleToken(modelToken);
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, getExpectedCards(cards, pageCards, 4));
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isTrue();

        modelProvider.handleToken(tokenFeature.getModelToken());
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, getExpectedCards(cards, pageCards, -1));
        assertThat(tokenFeature).isNull();
    }

    /**
     * Test Synthetic tokens on paged results when creating an existing session.
     *
     * <ol>
     *   <li>Create an initial $HEAD with 4 items and a second page of 8 additional items
     *   <li>Create a new session and validate the expected cards and a real next page token.
     *   <li>Handle the token to bring in the second page of items
     *   <li>Validate that we have 8 total items, plus a synthetic token
     *   <li>Recreate the session (invalidating the first version)
     *   <li>Validate that we have 4 cards and a synthetic token
     *   <li>Handle the synthetic token
     *   <li>Validate that we have 8 total items, plus a synthetic token
     *   <li>Handle the synthetic token
     *   <li>Validate that we have all 12 total items with no token
     * </ol>
     */
    @Test
    public void syntheticTokenPaging_pagingRestoredSession() {
        ContentId[] cards = new ContentId[] {
                ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
        };
        ContentId[] pageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(5),
                ResponseBuilder.createFeatureContentId(6),
                ResponseBuilder.createFeatureContentId(7),
                ResponseBuilder.createFeatureContentId(8),
                ResponseBuilder.createFeatureContentId(9),
                ResponseBuilder.createFeatureContentId(10),
                ResponseBuilder.createFeatureContentId(11),
                ResponseBuilder.createFeatureContentId(12)};

        PagingState state = new PagingState(cards, pageCards, 1, contentIdGenerators);
        fakeFeedRequestManager.queueResponse(state.initialResponse);
        fakeFeedRequestManager.queueResponse(state.pageResponse);
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        String sessionId = modelProvider.getSessionId();

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        ModelChild tokenFeature = modelValidator.assertCursorContentsWithToken(cursor, cards);
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isFalse();

        ModelToken modelToken = tokenFeature.getModelToken();
        modelProvider.handleToken(modelToken);
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, getExpectedCards(cards, pageCards, 4));
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isTrue();

        // now restore the session and see what we have
        modelProvider = modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(cursor, cards);
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isTrue();

        modelProvider.handleToken(tokenFeature.getModelToken());
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, getExpectedCards(cards, pageCards, 4));
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isTrue();

        modelProvider.handleToken(tokenFeature.getModelToken());
        cursor = root.getCursor();
        tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, getExpectedCards(cards, pageCards, -1));
        assertThat(tokenFeature).isNull();
    }

    /**
     * This will tests synthetic tokens in the context of an empty ModelProvider, see [INTERNAL
     * LINK]. This test is written directly against the session manager instead of the request
     * manager.
     *
     * <p>
     * <li>Create an initial Model provider with a single content item under the root
     * <li>Remove that item, creating an empty Root
     * <li>Added a new page of items with enough to cause a synthetic token to be added
     */
    @Test
    public void testEmptyFeed() {
        ContentId[] cards = new ContentId[] {
                ResponseBuilder.createFeatureContentId(1),
        };
        ContentId[] cardsTwo = new ContentId[] {
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
                ResponseBuilder.createFeatureContentId(5),
                ResponseBuilder.createFeatureContentId(6),
                ResponseBuilder.createFeatureContentId(7),
                ResponseBuilder.createFeatureContentId(8),
        };

        // Create an initial root with a single item in it.
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);

        // Prep for sending modifications to the model provider
        MutationContext context = new MutationContext.Builder()
                                          .setRequestingSessionId(modelProvider.getSessionId())
                                          .build();

        // Remove the single item under the root to create the empty state.
        Result<Model> result = protocolAdapter.createModel(
                ResponseBuilder.builder().removeFeature(cards[0], ROOT_CONTENT_ID).build());
        assertThat(result.isSuccessful()).isTrue();
        Consumer<Result<Model>> updateConsumer = feedSessionManager.getUpdateConsumer(context);
        updateConsumer.accept(result);

        // Verify the empty state
        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        assertThat(cursor.isAtEnd()).isTrue();

        // Create a next page response with enough new cards to ensure a synthetic token is added
        result = protocolAdapter.createModel(
                ResponseBuilder.builder().addCardsToRoot(cardsTwo).build());
        assertThat(result.isSuccessful()).isTrue();
        updateConsumer = feedSessionManager.getUpdateConsumer(context);
        updateConsumer.accept(result);

        // Validate the current model
        root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        cursor = root.getCursor();
        ModelChild tokenFeature = modelValidator.assertCursorContentsWithToken(
                cursor, Arrays.copyOf(cardsTwo, INITIAL_PAGE_SIZE));
        assertThat(tokenFeature).isNotNull();
        assertThat(tokenFeature.getModelToken().isSynthetic()).isTrue();
    }

    private ContentId[] getExpectedCards(ContentId[] first, ContentId[] second, int length) {
        if (length == -1) {
            length = second.length;
        }
        ContentId[] results = new ContentId[first.length + length];
        System.arraycopy(first, 0, results, 0, first.length);
        System.arraycopy(second, 0, results, first.length, length);
        return results;
    }

    private void clearSessionManagerContentCache() {
        // This is a bit of hack, which will clear the content cache and indicate to the
        // FeedSessionManager that we are in the non-cached mode.
        fakeFeedRequestManager.queueResponse(new ResponseBuilder().build());
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        fakeClock.advance(TaskQueue.STARVATION_TIMEOUT_MS);
    }
}
