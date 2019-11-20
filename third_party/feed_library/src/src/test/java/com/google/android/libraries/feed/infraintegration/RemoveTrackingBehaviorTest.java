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

import static com.google.android.libraries.feed.common.testing.ResponseBuilder.ROOT_CONTENT_ID;
import static com.google.common.truth.Truth.assertThat;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.RemoveTracking;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Function;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.ResponseProto.Response;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the ModelProvider RemoveTracking behavior. */
@RunWith(RobolectricTestRunner.class)
public class RemoveTrackingBehaviorTest {
  private static final ContentId[] CARDS =
      new ContentId[] {
        ResponseBuilder.createFeatureContentId(1),
        ResponseBuilder.createFeatureContentId(2),
        ResponseBuilder.createFeatureContentId(3),
        ResponseBuilder.createFeatureContentId(4),
        ResponseBuilder.createFeatureContentId(5),
      };

  private FakeFeedRequestManager fakeFeedRequestManager;
  private FeedSessionManager feedSessionManager;
  private ModelProviderFactory modelProviderFactory;
  private ProtocolAdapter protocolAdapter;
  private RequestManager requestManager;

  @Before
  public void setUp() {
    initMocks(this);
    InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    fakeFeedRequestManager = scope.getFakeFeedRequestManager();
    feedSessionManager = scope.getFeedSessionManager();
    modelProviderFactory = scope.getModelProviderFactory();
    protocolAdapter = scope.getProtocolAdapter();
    requestManager = scope.getRequestManager();
  }

  @Test
  public void testBaseRemoveTracking() {
    loadInitialData();

    AtomicBoolean called = new AtomicBoolean(false);
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
    modelProvider.enableRemoveTracking(
        getRemoveTrackingFactory(
            (contentIds) -> {
              assertThat(contentIds).hasSize(1);
              called.set(true);
            }));
    ResponseBuilder responseBuilder =
        ResponseBuilder.builder().removeFeature(CARDS[1], ROOT_CONTENT_ID);
    List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
    MutationContext mutationContext = new MutationContext.Builder().setUserInitiated(true).build();
    Consumer<Result<Model>> updateConsumer = feedSessionManager.getUpdateConsumer(mutationContext);
    updateConsumer.accept(Result.success(Model.of(dataOperations)));
    assertThat(called.get()).isTrue();
  }

  @Test
  public void testBaseRemoveTracking_multipleItems() {
    loadInitialData();

    AtomicBoolean called = new AtomicBoolean(false);
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
    modelProvider.enableRemoveTracking(
        getRemoveTrackingFactory(
            (contentIds) -> {
              assertThat(contentIds).hasSize(2);
              called.set(true);
            }));
    ResponseBuilder responseBuilder =
        ResponseBuilder.builder()
            .removeFeature(CARDS[1], ROOT_CONTENT_ID)
            .removeFeature(CARDS[3], ROOT_CONTENT_ID);
    List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
    MutationContext mutationContext = new MutationContext.Builder().setUserInitiated(true).build();
    Consumer<Result<Model>> updateConsumer = feedSessionManager.getUpdateConsumer(mutationContext);
    updateConsumer.accept(Result.success(Model.of(dataOperations)));
    assertThat(called.get()).isTrue();
  }

  /**
   * For non-user initiated mutations, the test is setup to return null from the factory. The result
   * is the Consumer should not be called.
   */
  @Test
  public void testNonUserInitiated() {
    loadInitialData();

    AtomicBoolean called = new AtomicBoolean(false);
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
    modelProvider.enableRemoveTracking(
        getRemoveTrackingFactory(
            (contentIds) -> {
              assertThat(contentIds).isEmpty();
              called.set(true);
            }));

    ResponseBuilder responseBuilder =
        ResponseBuilder.builder().removeFeature(CARDS[1], ROOT_CONTENT_ID);
    List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
    MutationContext mutationContext = new MutationContext.Builder().setUserInitiated(false).build();
    Consumer<Result<Model>> updateConsumer = feedSessionManager.getUpdateConsumer(mutationContext);
    updateConsumer.accept(Result.success(Model.of(dataOperations)));
    assertThat(called.get()).isFalse();
  }

  private RemoveTrackingFactory<String> getRemoveTrackingFactory(Consumer<List<String>> consumer) {
    return new RemoveTrackingFactory<String>() {
      @Override
      public /*@Nullable*/ RemoveTracking<String> create(MutationContext mutationContext) {
        // Only support RemoveTracking on user initiated removes
        return mutationContext.isUserInitiated()
            ? getRemoveTracking(
                (streamFeature) -> simpleTransform(streamFeature),
                consumer)
            : null;
      }
    };
  }

  private RemoveTracking<String> getRemoveTracking(
      Function<StreamFeature, String> transformer,
      Consumer<List<String>> consumer) {
    return new RemoveTracking<>(transformer, consumer);
  }

  @SuppressWarnings("unused")
  private boolean alwaysTrue(String value) {
    return true;
  }

  private String simpleTransform(StreamFeature streamFeature) {
    // only return the Content StreamFeatures.
    return streamFeature.hasContent() ? streamFeature.getContentId() : null;
  }

  private List<StreamDataOperation> getDataOperations(ResponseBuilder builder) {
    Response response = builder.build();
    Result<Model> result = protocolAdapter.createModel(response);
    assertThat(result.isSuccessful()).isTrue();
    return result.getValue().streamDataOperations;
  }

  private void loadInitialData() {
    fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(CARDS).build());
    requestManager.triggerScheduledRefresh();
  }
}
