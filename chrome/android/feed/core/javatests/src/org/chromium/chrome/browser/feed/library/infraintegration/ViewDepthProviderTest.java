// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.SessionState;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeViewDepthProvider;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests of the ViewDepthProvider. The ViewDepthProvider indicates the depth of the Stream the user
 * has seen. For some types of SchedulerApi {@link RequestBehavior} values this will prune the
 * Stream beyond that depth.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewDepthProviderTest {
    private static final ContentId[] REQUEST_ONE = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3), ResponseBuilder.createFeatureContentId(4)};
    private static final ContentId[] REQUEST_TWO = new ContentId[] {
            ResponseBuilder.createFeatureContentId(5), ResponseBuilder.createFeatureContentId(6),
            ResponseBuilder.createFeatureContentId(7)};
    private static final ContentId[] REQUEST_TWO_WITH_DUPLICATES = new ContentId[] {
            ResponseBuilder.createFeatureContentId(5), ResponseBuilder.createFeatureContentId(6),
            ResponseBuilder.createFeatureContentId(4)};
    private static final ContentId[] REQUEST_TWO_WITH_DUPLICATES_PAGE = new ContentId[] {
            ResponseBuilder.createFeatureContentId(5), ResponseBuilder.createFeatureContentId(4),
            ResponseBuilder.createFeatureContentId(7)};

    @Mock
    private SchedulerApi mSchedulerApi;

    private FakeFeedRequestManager mFakeFeedRequestManager;
    private FakeThreadUtils mFakeThreadUtils;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private FeedSessionManager mFeedSessionManager;
    private ViewDepthProvider mViewDepthProvider;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder()
                                              .setSchedulerApi(mSchedulerApi)
                                              .withTimeoutSessionConfiguration(2L)
                                              .build();
        mFakeThreadUtils = scope.getFakeThreadUtils();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        ProtocolAdapter protocolAdapter = scope.getProtocolAdapter();
        mModelValidator = new ModelProviderValidator(protocolAdapter);
        mFeedSessionManager = scope.getFeedSessionManager();
        mViewDepthProvider = new FakeViewDepthProvider().setChildViewDepth(
                protocolAdapter.getStreamContentId(REQUEST_ONE[1]));
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void baseDepthProviderTest() {
        // Load up the initial request
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build());

        // The REQUEST_ONE content will be added to head, this is then used to create the initial
        // session.
        mRequestManager.triggerScheduledRefresh();

        // The REQUEST_TWO content acts as a second request on the server, it is triggered by
        // REQUEST_WITH_CONTENT
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO).build());
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(mViewDepthProvider, UiContext.getDefaultInstance());

        // The second request will be added after the first request, the ViewDepthProvider indicates
        // we only saw [0] and [1] from the first request, so [2] and [3] will be removed.
        mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO[0], REQUEST_TWO[1], REQUEST_TWO[2]);
    }

    @Test
    public void testDuplicateEntries() {
        // Load up the initial request
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build());

        // The REQUEST_ONE content will be added to head, this is then used to create the initial
        // session.
        mRequestManager.triggerScheduledRefresh();

        // The REQUEST_TWO content acts as a second request on the server, it is triggered by
        // REQUEST_WITH_CONTENT
        when(mSchedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO_WITH_DUPLICATES).build());
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(mViewDepthProvider, UiContext.getDefaultInstance());

        // The second request will be added after the first request, the ViewDepthProvider indicates
        // we only saw [0] and [1] from the first request, so [2] and [3] will be removed.
        mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO_WITH_DUPLICATES[0], REQUEST_TWO_WITH_DUPLICATES[1],
                REQUEST_TWO_WITH_DUPLICATES[2]);

        // Now page in the same content, this should all be updates
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(REQUEST_TWO_WITH_DUPLICATES_PAGE).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO_WITH_DUPLICATES[0], REQUEST_TWO_WITH_DUPLICATES[1],
                REQUEST_TWO_WITH_DUPLICATES[2], REQUEST_TWO_WITH_DUPLICATES_PAGE[2]);
    }
}
