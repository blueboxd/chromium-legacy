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

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.protobuf.ByteString;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.ResponseProto.Response;
import java.util.Collections;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests around Semantic Properties */
@RunWith(RobolectricTestRunner.class)
public class SemanticPropertiesTest {
  private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
  private final FakeFeedRequestManager fakeFeedRequestManager = scope.getFakeFeedRequestManager();
  private final RequestManager requestManager = scope.getRequestManager();
  private final Store store = scope.getStore();

  @Test
  public void persistingSemanticProperties() {
    ContentId contentId = ResponseBuilder.createFeatureContentId(13);
    ByteString semanticData = ByteString.copyFromUtf8("helloWorld");

    Response response =
        new ResponseBuilder()
            .addClearOperation()
            .addCardWithSemanticData(contentId, semanticData)
            .build();
    fakeFeedRequestManager.queueResponse(response);
    requestManager.triggerScheduledRefresh();

    scope.getFakeThreadUtils().enforceMainThread(false);
    ContentIdGenerators idGenerators = new ContentIdGenerators();
    String contentIdString = idGenerators.createContentId(contentId);
    Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
        store.getSemanticProperties(Collections.singletonList(contentIdString));
    assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
    List<SemanticPropertiesWithId> semanticProperties = semanticPropertiesResult.getValue();
    assertThat(semanticProperties).hasSize(1);
    assertThat(semanticProperties.get(0).contentId).isEqualTo(contentIdString);
    assertThat(semanticProperties.get(0).semanticData).isEqualTo(semanticData.toByteArray());
  }
}
