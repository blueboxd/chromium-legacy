// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.PagingState;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.WireProtocolInfo;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

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
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiSessionPagingTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private FeedSessionManager mFeedSessionManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mFeedSessionManager = scope.getFeedSessionManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void testPaging() {
        // Create session 1 content
        ContentId[] s1Cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ContentId[] s1PageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        PagingState s1State = new PagingState(s1Cards, s1PageCards, 1, mContentIdGenerators);
        ByteString token1 = ByteString.copyFrom("s1-page", Charset.defaultCharset());
        ResponseBuilder s1InitialResponse = getInitialResponse(s1Cards, token1);

        // Create session 2 content
        ContentId[] s2Cards = new ContentId[] {ResponseBuilder.createFeatureContentId(101),
                ResponseBuilder.createFeatureContentId(102)};
        ContentId[] s2PageCards = new ContentId[] {ResponseBuilder.createFeatureContentId(103),
                ResponseBuilder.createFeatureContentId(104)};
        PagingState s2State = new PagingState(s2Cards, s2PageCards, 2, mContentIdGenerators);

        // Create an initial S1 $HEAD session
        mFakeFeedRequestManager.queueResponse(s1State.initialResponse);
        mRequestManager.triggerScheduledRefresh();

        ModelProvider mp1 = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertRoot(mp1);
        WireProtocolInfo protocolInfo = s1InitialResponse.getWireProtocolInfo();
        assertThat(protocolInfo.hasToken).isTrue();
        ModelChild mp1Token = mModelValidator.assertCursorContentsWithToken(mp1, s1Cards);
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);

        // Create a second session against the S1 head.
        ModelProvider mp2 = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(mp2.getCurrentState()).isEqualTo(State.READY);
        mModelValidator.assertCursorContentsWithToken(mp2, s1Cards);

        // Refresh the Stream with the S2 initial response
        mFakeFeedRequestManager.queueResponse(s2State.initialResponse);
        mFakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                mFeedSessionManager.getUpdateConsumer(
                        new MutationContext.Builder()
                                .setRequestingSessionId(mp2.getSessionId())
                                .build()));
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);
        assertThat(mp2.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Now create a ModelProvider against the new session.
        mp2 = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertRoot(mp2);
        protocolInfo = s1InitialResponse.getWireProtocolInfo();
        assertThat(protocolInfo.hasToken).isTrue();
        ModelChild mp2Token = mModelValidator.assertCursorContentsWithToken(mp2, s2Cards);
        assertThat(mp2.getCurrentState()).isEqualTo(State.READY);

        // Verify that we didn't change the first session
        mModelValidator.assertCursorContentsWithToken(mp1, s1Cards);
        assertThat(mp1.getCurrentState()).isEqualTo(State.READY);

        // now page S1
        mFakeFeedRequestManager.queueResponse(s1State.pageResponse);
        mp1.handleToken(mp1Token.getModelToken());
        mModelValidator.assertCursorContents(
                mp1, s1Cards[0], s1Cards[1], s1PageCards[0], s1PageCards[1]);
        mModelValidator.assertCursorContentsWithToken(mp2, s2Cards);

        // now page S2
        mFakeFeedRequestManager.queueResponse(s2State.pageResponse);
        mp2.handleToken(mp2Token.getModelToken());
        mModelValidator.assertCursorContents(
                mp1, s1Cards[0], s1Cards[1], s1PageCards[0], s1PageCards[1]);
        mModelValidator.assertCursorContents(
                mp2, s2Cards[0], s2Cards[1], s2PageCards[0], s2PageCards[1]);
    }

    private ResponseBuilder getInitialResponse(ContentId[] cards, ByteString token) {
        return ResponseBuilder.forClearAllWithCards(cards).addStreamToken(1, token);
    }
}
