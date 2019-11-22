// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of accessing shared state. */
@RunWith(RobolectricTestRunner.class)
public class SharedStateTest {
    private FakeFeedRequestManager fakeFeedRequestManager;
    private ProtocolAdapter protocolAdapter;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        protocolAdapter = scope.getProtocolAdapter();
        modelValidator = new ModelProviderValidator(protocolAdapter);
        requestManager = scope.getRequestManager();
    }

    @Test
    public void sharedState_headBeforeModelProvider() {
        // ModelProvider is created from $HEAD containing content, simple shared state added
        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addPietSharedState().addRootFeature();
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();

        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        StreamSharedState sharedState =
                modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNotNull();
        modelValidator.assertStreamContentId(sharedState.getContentId(),
                protocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
    }

    @Test
    public void sharedState_headAfterModelProvider() {
        // ModelProvider is created from empty $HEAD, simple shared state added
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        StreamSharedState sharedState =
                modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNull();

        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addPietSharedState().addRootFeature();
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();

        sharedState = modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNotNull();
        modelValidator.assertStreamContentId(sharedState.getContentId(),
                protocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
    }
}
