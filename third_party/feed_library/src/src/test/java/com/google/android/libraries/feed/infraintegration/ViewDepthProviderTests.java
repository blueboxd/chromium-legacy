// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.SessionState;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.modelprovider.FakeViewDepthProvider;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests of the ViewDepthProvider. The ViewDepthProvider indicates the depth of the Stream the user
 * has seen. For some types of SchedulerApi {@link RequestBehavior} values this will prune the
 * Stream beyond that depth.
 */
@RunWith(RobolectricTestRunner.class)
public class ViewDepthProviderTests {
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
    private SchedulerApi schedulerApi;

    private FakeFeedRequestManager fakeFeedRequestManager;
    private FakeThreadUtils fakeThreadUtils;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private FeedSessionManager feedSessionManager;
    private ViewDepthProvider viewDepthProvider;
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder()
                                              .setSchedulerApi(schedulerApi)
                                              .withTimeoutSessionConfiguration(2L)
                                              .build();
        fakeThreadUtils = scope.getFakeThreadUtils();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        ProtocolAdapter protocolAdapter = scope.getProtocolAdapter();
        modelValidator = new ModelProviderValidator(protocolAdapter);
        feedSessionManager = scope.getFeedSessionManager();
        viewDepthProvider = new FakeViewDepthProvider().setChildViewDepth(
                protocolAdapter.getStreamContentId(REQUEST_ONE[1]));
        requestManager = scope.getRequestManager();
    }

    @Test
    public void baseDepthProviderTest() {
        // Load up the initial request
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build());

        // The REQUEST_ONE content will be added to head, this is then used to create the initial
        // session.
        requestManager.triggerScheduledRefresh();

        // The REQUEST_TWO content acts as a second request on the server, it is triggered by
        // REQUEST_WITH_CONTENT
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO).build());
        ModelProvider modelProvider =
                modelProviderFactory.createNew(viewDepthProvider, UiContext.getDefaultInstance());

        // The second request will be added after the first request, the ViewDepthProvider indicates
        // we only saw [0] and [1] from the first request, so [2] and [3] will be removed.
        modelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO[0], REQUEST_TWO[1], REQUEST_TWO[2]);
    }

    @Test
    public void testDuplicateEntries() {
        // Load up the initial request
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build());

        // The REQUEST_ONE content will be added to head, this is then used to create the initial
        // session.
        requestManager.triggerScheduledRefresh();

        // The REQUEST_TWO content acts as a second request on the server, it is triggered by
        // REQUEST_WITH_CONTENT
        when(schedulerApi.shouldSessionRequestData(any(SessionState.class)))
                .thenReturn(RequestBehavior.REQUEST_WITH_CONTENT);
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO_WITH_DUPLICATES).build());
        ModelProvider modelProvider =
                modelProviderFactory.createNew(viewDepthProvider, UiContext.getDefaultInstance());

        // The second request will be added after the first request, the ViewDepthProvider indicates
        // we only saw [0] and [1] from the first request, so [2] and [3] will be removed.
        modelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO_WITH_DUPLICATES[0], REQUEST_TWO_WITH_DUPLICATES[1],
                REQUEST_TWO_WITH_DUPLICATES[2]);

        // Now page in the same content, this should all be updates
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(REQUEST_TWO_WITH_DUPLICATES_PAGE).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        fakeThreadUtils.enforceMainThread(false);
        fakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        modelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0], REQUEST_ONE[1],
                REQUEST_TWO_WITH_DUPLICATES[0], REQUEST_TWO_WITH_DUPLICATES[1],
                REQUEST_TWO_WITH_DUPLICATES[2], REQUEST_TWO_WITH_DUPLICATES_PAGE[2]);
    }
}
