// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.PagingState;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** This will test detaching a session then updating it and reattaching to it */
@RunWith(RobolectricTestRunner.class)
public class DetachSessionTest {
    // Create a simple stream with a root and three features
    private static final ContentId[] PAGE_1 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};
    private static final ContentId[] PAGE_2 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(4), ResponseBuilder.createFeatureContentId(5),
            ResponseBuilder.createFeatureContentId(6)};

    private FakeFeedRequestManager fakeFeedRequestManager;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private RequestManager requestManager;

    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();

    @Before
    public void setUp() {
        initMocks(this);
        Configuration configuration =
                new Configuration.Builder().put(ConfigKey.LIMIT_PAGE_UPDATES, false).build();
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder().setConfiguration(configuration).build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        requestManager = scope.getRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
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
        PagingState s1State = new PagingState(PAGE_1, PAGE_2, 1, contentIdGenerators);

        fakeFeedRequestManager.queueResponse(s1State.initialResponse);
        requestManager.triggerScheduledRefresh();

        // Create the sessionOne from page1 and a token
        ModelProvider sessionOne =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        ModelChild tokenChild = modelValidator.assertCursorContentsWithToken(sessionOne, PAGE_1);

        // Create the sessionTwo matching sessionOne
        ModelProvider sessionTwo =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Detach sessionOne
        String sessionId = sessionOne.getSessionId();
        sessionOne.detachModelProvider();

        // recreate sessionOne as existingSession
        ModelProvider existingSession =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        modelValidator.assertCursorContentsWithToken(existingSession, PAGE_1);

        // page the sessionTwo
        fakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());

        // verify that existingSession and sessionTwo match
        modelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        modelValidator.assertCursorContents(
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
        PagingState s1State = new PagingState(PAGE_1, PAGE_2, 1, contentIdGenerators);

        fakeFeedRequestManager.queueResponse(s1State.initialResponse);
        requestManager.triggerScheduledRefresh();

        // Create the sessionOne from page1 and a token
        ModelProvider sessionOne =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        ModelChild tokenChild = modelValidator.assertCursorContentsWithToken(sessionOne, PAGE_1);

        // Create the sessionTwo matching sessionOne
        ModelProvider sessionTwo =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Detach sessionOne
        String sessionId = sessionOne.getSessionId();
        sessionOne.detachModelProvider();

        // page the sessionTwo
        fakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());

        // recreate sessionOne as existingSession
        ModelProvider existingSession =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());

        // verify that existingSession and sessionTwo match
        modelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        modelValidator.assertCursorContents(
                existingSession, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
    }
}
