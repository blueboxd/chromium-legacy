// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompleted;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.PagingState;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test of handling paging operations within the Stream. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamPagingTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
        mRequestManager = scope.getRequestManager();
    }

    /**
     * This test will create a Session and page it. Fully validating the observer and results.
     *
     * <ol>
     *   <li>Setup the initial response and the paged response
     *   <li>Setup the paging handling for the FeedRequestManager
     *   <li>Create the Initial $HEAD from the initial response
     *   <li>Create a Session/ModelProvider
     *   <li>Setup the MutationContext and the ModelToken observer
     *   <li>ModelProvider.handleToken to cause the paging response to be loaded
     *   <li>Validate...
     * </ol>
     */
    @Test
    public void testPaging() {
        // ModelProvider created after $HEAD has content, one root, and two Cards and a token
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ContentId[] pageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        PagingState state = new PagingState(cards, pageCards, 1, mContentIdGenerators);
        mFakeFeedRequestManager.queueResponse(state.initialResponse);
        mFakeFeedRequestManager.queueResponse(state.pageResponse);

        // Create an initial model
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertRoot(modelProvider);

        // Validate the structure of the stream after the initial response
        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor cursor = rootFeature.getCursor();
        ModelChild tokenFeature = mModelValidator.assertCursorContentsWithToken(cursor, cards);
        assertThat(tokenFeature).isNotNull();

        // Add an observer to the Token to get an event when the response is processed
        TokenCompletedObserver tokenCompletedObserver = mock(TokenCompletedObserver.class);
        ModelToken modelToken = tokenFeature.getModelToken();
        assertThat(modelToken).isNotNull();

        // Capture the event triggered once the paging operation is finished
        modelToken.registerObserver(tokenCompletedObserver);
        modelProvider.handleToken(modelToken);
        ArgumentCaptor<TokenCompleted> completedArgumentCaptor =
                ArgumentCaptor.forClass(TokenCompleted.class);
        verify(tokenCompletedObserver).onTokenCompleted(completedArgumentCaptor.capture());
        ModelCursor pageCursor = completedArgumentCaptor.getValue().getCursor();
        assertThat(pageCursor).isNotNull();

        // The event cursor will have only the new items added in the page
        mModelValidator.assertCursorContents(pageCursor, pageCards);

        // The full cursor should now contain all the features
        cursor = rootFeature.getCursor();
        mModelValidator.assertCursorContents(
                cursor, cards[0], cards[1], pageCards[0], pageCards[1]);
    }

    /**
     * This test will create a session and page it. Then create a new session from $HEAD to verify
     * that Head is correctly handling the token add/remove combination.
     *
     * <ul>
     *   <li>Setup the initial response and the paged response
     *   <li>Setup the paging handling for the FeedRequestManager
     *   <li>Create the Initial $HEAD from the initial response
     *   <li>Create a Session/ModelProvider
     *   <li>Page the Session/ModelProvider
     *   <li>Create a new Session from $HEAD
     *   <li>Validate that the PageToken are not in the cursor
     * </ul>
     */
    @Test
    public void testPostPagingSessionCreation() {
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ContentId[] pageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        PagingState state = new PagingState(cards, pageCards, 1, mContentIdGenerators);
        mFakeFeedRequestManager.queueResponse(state.initialResponse);
        mFakeFeedRequestManager.queueResponse(state.pageResponse);

        // Create an initial model
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor cursor = rootFeature.getCursor();
        ModelChild tokenFeature = mModelValidator.assertCursorContentsWithToken(cursor, cards);
        modelProvider.handleToken(tokenFeature.getModelToken());
        cursor = rootFeature.getCursor();
        mModelValidator.assertCursorContents(
                cursor, cards[0], cards[1], pageCards[0], pageCards[1]);

        // Now create a second ModelProvider and verify the cursor.
        ModelProvider session2 =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        rootFeature = session2.getRootFeature();
        assertThat(rootFeature).isNotNull();
        cursor = rootFeature.getCursor();
        mModelValidator.assertCursorContents(
                cursor, cards[0], cards[1], pageCards[0], pageCards[1]);
    }
}
