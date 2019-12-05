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

/** Test which verifies that multiple session page correctly when limited paging is on. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LimitedPagingTest {
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
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
        mRequestManager = scope.getRequestManager();
    }

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

        // Now Page session one and verify both session are as expected, only session one will
        // contain the paged content
        mFakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionOne.handleToken(tokenChild.getModelToken());

        // Create the sessionOne from page1 and a token
        mModelValidator.assertCursorContents(
                sessionOne, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        mModelValidator.assertCursorContentsWithToken(sessionTwo, PAGE_1);

        // Now create a new session from $HEAD to verify that the page response was added to $HEAD
        ModelProvider sessionThree =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContents(
                sessionThree, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);

        // Page session two, this should not change anything else
        mFakeFeedRequestManager.queueResponse(s1State.pageResponse);
        sessionTwo.handleToken(tokenChild.getModelToken());
        mModelValidator.assertCursorContents(
                sessionOne, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        mModelValidator.assertCursorContents(
                sessionTwo, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
        mModelValidator.assertCursorContents(
                sessionThree, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);

        // Verify that paging session two did not update $HEAD
        ModelProvider sessionFour =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContents(
                sessionFour, PAGE_1[0], PAGE_1[1], PAGE_1[2], PAGE_2[0], PAGE_2[1], PAGE_2[2]);
    }
}
