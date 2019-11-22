// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.PagingState;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.testing.ResponseBuilder.WireProtocolInfo;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.nio.charset.Charset;

/**
 * This test will create multiple sessions with different content in each. It will then page each
 * session with different tokens to verify that paging sessions doesn't affect sessions without the
 * paging token.
 *
 * <p>The test runs the following tasks:
 *
 * <ol>
 *   <li>Create the initial response and page response for Session 1 and 2, including tokens
 *   <li>Create Session 1 and $HEAD from Session 1 initial response
 *   <li>Create Session 2 from $HEAD (Session 1 initial response)
 *   <li>Refresh Session 2 (directly using the protocol adapter) using Session 2 initial response.
 *       This creates a new $HEAD and invalidates the Session 2 Model Provider
 *   <li>Create a new Session 2 against the new $HEAD
 *   <li>Validate that Session 1 and 2 have the expected content
 *   <li>Page Session 1 and validate that both sessions have expected content
 *   <li>Page Session 2 and validate that both sessions have expected content
 * </ol>
 */
@RunWith(RobolectricTestRunner.class)
public class MultiSessionPagingTest {
    private FakeFeedRequestManager fakeFeedRequestManager;
    private FeedSessionManager feedSessionManager;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private ContentIdGenerators contentIdGenerators = new ContentIdGenerators();
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        feedSessionManager = scope.getFeedSessionManager();
        modelProviderFactory = scope.getModelProviderFactory();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
        requestManager = scope.getRequestManager();
    }

    @Test
    public void testPaging() {
        // Create session 1 content
        ContentId[] s1Cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ContentId[] s1PageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        PagingState s1State = new PagingState(s1Cards, s1PageCards, 1, contentIdGenerators);
        ByteString token1 = ByteString.copyFrom("s1-page", Charset.defaultCharset());
        ResponseBuilder s1InitialResponse = getInitialResponse(s1Cards, token1);

        // Create session 2 content
        ContentId[] s2Cards = new ContentId[] {ResponseBuilder.createFeatureContentId(101),
                ResponseBuilder.createFeatureContentId(102)};
        ContentId[] s2PageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(103),
                ResponseBuilder.createFeatureContentId(104)};
        PagingState s2State = new PagingState(s2Cards, s2PageCards, 2, contentIdGenerators);

        // Create an initial S1 $HEAD session
        fakeFeedRequestManager.queueResponse(s1State.initialResponse);
        requestManager.triggerScheduledRefresh();

        ModelProvider mp1 = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertRoot(mp1);
        WireProtocolInfo protocolInfo = s1InitialResponse.getWireProtocolInfo();
        assertThat(protocolInfo.hasToken).isTrue();
        ModelChild mp1Token = modelValidator.assertCursorContentsWithToken(mp1, s1Cards);
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);

        // Create a second session against the S1 head.
        ModelProvider mp2 = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(mp2.getCurrentState()).isEqualTo(State.READY);
        modelValidator.assertCursorContentsWithToken(mp2, s1Cards);

        // Refresh the Stream with the S2 initial response
        fakeFeedRequestManager.queueResponse(s2State.initialResponse);
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(
                        new MutationContext.Builder()
                                .setRequestingSessionId(mp2.getSessionId())
                                .build()));
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);
        assertThat(mp2.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Now create a ModelProvider against the new session.
        mp2 = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertRoot(mp2);
        protocolInfo = s1InitialResponse.getWireProtocolInfo();
        assertThat(protocolInfo.hasToken).isTrue();
        ModelChild mp2Token = modelValidator.assertCursorContentsWithToken(mp2, s2Cards);
        assertThat(mp2.getCurrentState()).isEqualTo(State.READY);

        // Verify that we didn't change the first session
        modelValidator.assertCursorContentsWithToken(mp1, s1Cards);
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);

        // now page S1
        fakeFeedRequestManager.queueResponse(s1State.pageResponse);
        mp1.handleToken(mp1Token.getModelToken());
        modelValidator.assertCursorContents(
                mp1, s1Cards[0], s1Cards[1], s1PageCards[0], s1PageCards[1]);
        modelValidator.assertCursorContentsWithToken(mp2, s2Cards);

        // now page S2
        fakeFeedRequestManager.queueResponse(s2State.pageResponse);
        mp2.handleToken(mp2Token.getModelToken());
        modelValidator.assertCursorContents(
                mp1, s1Cards[0], s1Cards[1], s1PageCards[0], s1PageCards[1]);
        modelValidator.assertCursorContents(
                mp2, s2Cards[0], s2Cards[1], s2PageCards[0], s2PageCards[1]);
    }

    private ResponseBuilder getInitialResponse(ContentId[] cards, ByteString token) {
        return ResponseBuilder.forClearAllWithCards(cards).addStreamToken(1, token);
    }
}
