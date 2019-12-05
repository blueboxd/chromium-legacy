// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of accessing shared state. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SharedStateTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ProtocolAdapter mProtocolAdapter;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mProtocolAdapter = scope.getProtocolAdapter();
        mModelValidator = new ModelProviderValidator(mProtocolAdapter);
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void sharedState_headBeforeModelProvider() {
        // ModelProvider is created from $HEAD containing content, simple shared state added
        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addPietSharedState().addRootFeature();
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();

        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        StreamSharedState sharedState =
                modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNotNull();
        mModelValidator.assertStreamContentId(sharedState.getContentId(),
                mProtocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
    }

    @Test
    public void sharedState_headAfterModelProvider() {
        // ModelProvider is created from empty $HEAD, simple shared state added
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        StreamSharedState sharedState =
                modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNull();

        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addPietSharedState().addRootFeature();
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();

        sharedState = modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
        assertThat(sharedState).isNotNull();
        mModelValidator.assertStreamContentId(sharedState.getContentId(),
                mProtocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
    }
}
