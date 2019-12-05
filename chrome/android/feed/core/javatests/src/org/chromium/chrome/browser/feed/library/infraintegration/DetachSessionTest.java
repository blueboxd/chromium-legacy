// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.PagingState;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** This will test detaching a session then updating it and reattaching to it */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DetachSessionTest {
    // Create a simple stream with a root and three features
    private static final ContentId[] PAGE_1 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};
    private static final ContentId[] PAGE_2 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(4), ResponseBuilder.createFeatureContentId(5),
            ResponseBuilder.createFeatureContentId(6)};

    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private RequestManager mRequestManager;

    private final ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();

    @Before
    public void setUp() {
        initMocks(this);
        Configuration configuration =
                new Configuration.Builder().put(ConfigKey.LIMIT_PAGE_UPDATES, false).build();
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder().setConfiguration(configuration).build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mRequestManager = scope.getRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    /**
     * Steps in this test:
     *
     * <ul>
     *   <li>Create a sessionOne (ModelProvider) with a token
     *   <li>Create a sessionTwo matching sessionOne
     *   <li>Detach sessionOne
     *   <li>Create an existing session based upon sessionOne
     *   <li>page sessionTwo
     *   <li>Verify sessionTwo and the existingSession match
     * </ul>
     */
    @Test
    public void testPagingDetachedSession() {
        PagingState s1State = new PagingState(PAGE_1, PAGE_2, 1, mContentIdGenerators);

        mFakeFeedRequestManager.queueResponse(s1State.initialResponse);
        mRequestManager.triggerScheduledRefresh();

        // Create the sessionOne from page1 and a token
        ModelProvider sessionOne =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        ModelChild tokenChild = mModelValidator.assertCursorContentsWithToken(sessionOne, PAGE_1);

        // Create the sessionTwo matching sessionOne
        ModelProvider sessionTwo =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Detach sessionOne
        String sessionId = sessionOne.getSessionId();
        sessionOne.detachModelProvider();

        // recreate sessionOne as existingSession
        ModelProvider existingSession =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContentsWithToken(existingSession, PAGE_1);

        // page the sessionTwo
        mFakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());

        // verify that existingSession and sessionTwo match
        mModelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        mModelValidator.assertCursorContents(
                existingSession, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
    }

    /**
     * This test differs from the previous test by paging sessionTwo while sessionOne is detached
     * and then creating the existing session. These should still match.
     *
     * <ul>
     *   <li>Create a sessionOne (ModelProvider) with a token
     *   <li>Create a sessionTwo matching sessionOne
     *   <li>Detach sessionOne
     *   <li>page sessionTwo
     *   <li>Create an existing session based upon sessionOne
     *   <li>Verify sessionTwo and the existingSession match
     * </ul>
     */
    @Test
    public void testPagingDetachedSession_pageWhileDetached() {
        PagingState s1State = new PagingState(PAGE_1, PAGE_2, 1, mContentIdGenerators);

        mFakeFeedRequestManager.queueResponse(s1State.initialResponse);
        mRequestManager.triggerScheduledRefresh();

        // Create the sessionOne from page1 and a token
        ModelProvider sessionOne =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        ModelChild tokenChild = mModelValidator.assertCursorContentsWithToken(sessionOne, PAGE_1);

        // Create the sessionTwo matching sessionOne
        ModelProvider sessionTwo =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Detach sessionOne
        String sessionId = sessionOne.getSessionId();
        sessionOne.detachModelProvider();

        // page the sessionTwo
        mFakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());

        // recreate sessionOne as existingSession
        ModelProvider existingSession =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());

        // verify that existingSession and sessionTwo match
        mModelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        mModelValidator.assertCursorContents(
                existingSession, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
    }
}
