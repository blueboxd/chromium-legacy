// Copyright 2019 The Feed Authors.
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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;

/** Tests which update (append) content to an existing model. */
@RunWith(RobolectricTestRunner.class)
public class StructureUpdateTest {
  private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
  private final FakeFeedRequestManager fakeFeedRequestManager = scope.getFakeFeedRequestManager();
  private final FakeThreadUtils fakeThreadUtils = scope.getFakeThreadUtils();
  private final ModelProviderFactory modelProviderFactory = scope.getModelProviderFactory();
  private final ModelProviderValidator modelValidator =
      new ModelProviderValidator(scope.getProtocolAdapter());
  private final ProtocolAdapter protocolAdapter = scope.getProtocolAdapter();
  private final FeedSessionManager feedSessionManager = scope.getFeedSessionManager();
  private final RequestManager requestManager = scope.getRequestManager();

  @Test
  public void appendChildren() {
    // Create a simple stream with a root and two features
    ContentId[] startingCards =
        new ContentId[] {
          ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2)
        };
    // Define two features to be appended to the root
    ContentId[] appendedCards =
        new ContentId[] {
          ResponseBuilder.createFeatureContentId(3), ResponseBuilder.createFeatureContentId(4)
        };

    fakeFeedRequestManager.queueResponse(
        ResponseBuilder.forClearAllWithCards(startingCards).build());
    requestManager.triggerScheduledRefresh();
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

    ModelFeature root = modelProvider.getRootFeature();
    assertThat(root).isNotNull();
    FeatureChangeObserver rootObserver = mock(FeatureChangeObserver.class);
    root.registerObserver(rootObserver);
    modelValidator.assertCursorSize(root.getCursor(), startingCards.length);

    // Append new children to root
    fakeFeedRequestManager.queueResponse(
        ResponseBuilder.builder().addCardsToRoot(appendedCards).build());
    // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
    fakeThreadUtils.enforceMainThread(false);
    fakeFeedRequestManager.loadMore(
        StreamToken.getDefaultInstance(),
        ConsistencyToken.getDefaultInstance(),
        feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

    // assert the new state of the stream
    modelValidator.assertCursorSize(root.getCursor(), startingCards.length + appendedCards.length);
    ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
    verify(rootObserver).onChange(capture.capture());
    List<FeatureChange> featureChanges = capture.getAllValues();
    assertThat(featureChanges).hasSize(1);
    FeatureChange change = featureChanges.get(0);
    assertThat(change.getChildChanges().getAppendedChildren()).hasSize(appendedCards.length);
    assertThat(change.isFeatureChanged()).isFalse();
    int i = 0;
    for (ModelChild appendedChild : change.getChildChanges().getAppendedChildren()) {
      modelValidator.assertStreamContentId(
          appendedChild.getContentId(), protocolAdapter.getStreamContentId(appendedCards[i++]));
    }
  }

  @Test
  public void appendChildren_concurrentModification() {
    // Test which verifies the root can be updated while we advance a cursor (without
    // a ConcurrentModificationException
    // Create a simple stream with a root and two features
    ContentId[] startingCards =
        new ContentId[] {
          ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2)
        };
    // Define two features to be appended to the root
    ContentId[] appendedCards =
        new ContentId[] {
          ResponseBuilder.createFeatureContentId(3), ResponseBuilder.createFeatureContentId(4)
        };

    fakeFeedRequestManager.queueResponse(
        ResponseBuilder.forClearAllWithCards(startingCards).build());
    requestManager.triggerScheduledRefresh();
    ModelProvider modelProvider =
        modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

    ModelFeature root = modelProvider.getRootFeature();
    assertThat(root).isNotNull();
    FeatureChangeObserver rootObserver = mock(FeatureChangeObserver.class);
    root.registerObserver(rootObserver);
    ModelCursor cursor = root.getCursor();
    cursor.getNextItem();

    // Now append additional children to the stream (and cursor)
    fakeFeedRequestManager.queueResponse(
        ResponseBuilder.builder().addCardsToRoot(appendedCards).build());
    // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
    fakeThreadUtils.enforceMainThread(false);
    fakeFeedRequestManager.loadMore(
        StreamToken.getDefaultInstance(),
        ConsistencyToken.getDefaultInstance(),
        feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
    modelValidator.assertCursorSize(cursor, 3);
  }
}
