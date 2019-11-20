// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    StreamSharedState sharedState = modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
    assertThat(sharedState).isNotNull();
    modelValidator.assertStreamContentId(
        sharedState.getContentId(),
        protocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
  }

  @Test
  public void sharedState_headAfterModelProvider() {
    // ModelProvider is created from empty $HEAD, simple shared state added
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
    StreamSharedState sharedState = modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
    assertThat(sharedState).isNull();

    ResponseBuilder responseBuilder =
        new ResponseBuilder().addClearOperation().addPietSharedState().addRootFeature();
    fakeFeedRequestManager.queueResponse(responseBuilder.build());
    requestManager.triggerScheduledRefresh();

    sharedState = modelProvider.getSharedState(ResponseBuilder.PIET_SHARED_STATE);
    assertThat(sharedState).isNotNull();
    modelValidator.assertStreamContentId(
        sharedState.getContentId(),
        protocolAdapter.getStreamContentId(ResponseBuilder.PIET_SHARED_STATE));
  }
}
