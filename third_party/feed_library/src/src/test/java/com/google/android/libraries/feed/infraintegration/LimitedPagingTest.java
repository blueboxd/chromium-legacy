// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
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

/** Test which verifies that multiple session page correctly when limited paging is on. */
@RunWith(RobolectricTestRunner.class)
public class LimitedPagingTest {
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
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
        requestManager = scope.getRequestManager();
    }

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

        // Now Page session one and verify both session are as expected, only session one will
        // contain the paged content
        fakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionOne.handleToken(tokenChild.getModelToken());

        // Create the sessionOne from page1 and a token
        modelValidator.assertCursorContents(
                sessionOne, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        modelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Now create a new session from $HEAD to verify that the page response was added to $HEAD
        ModelProvider sessionThree =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContents(
                sessionThree, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);

        // Page session two, this should not change anything else
        fakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());
        modelValidator.assertCursorContents(
                sessionOne, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        modelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        modelValidator.assertCursorContents(
                sessionThree, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);

        // Verify that paging session two did not update $HEAD
        ModelProvider sessionFour =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContents(
                sessionFour, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
    }
}
