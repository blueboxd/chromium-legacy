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

package com.google.android.libraries.feed.api.internal.modelprovider;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Function;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link RemoveTracking} class. */
@RunWith(RobolectricTestRunner.class)
public class RemoveTrackingTest {
  private final ContentIdGenerators idGenerators = new ContentIdGenerators();
  private final String rootContentId = idGenerators.createRootContentId(0);

  @Before
  public void setUp() {
    initMocks(this);
  }

  @Test
  public void testEmpty() {
    RemoveTracking<String> removeTracking =
        getRemoveTracking(this::simpleTransform, (contentIds) -> assertThat(contentIds).hasSize(0));
    removeTracking.triggerConsumerUpdate();
  }

  @Test
  public void testMatch() {
    RemoveTracking<String> removeTracking =
        getRemoveTracking(this::simpleTransform, (contentIds) -> assertThat(contentIds).hasSize(1));
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    protocolBuilder.addFeature(idGenerators.createFeatureContentId(0), rootContentId);
    List<PayloadWithId> payloads = protocolBuilder.buildAsPayloadWithId();
    for (PayloadWithId payload : payloads) {
      assertThat(payload.payload.hasStreamFeature()).isTrue();
      removeTracking.filterStreamFeature(payload.payload.getStreamFeature());
    }
    removeTracking.triggerConsumerUpdate();
  }

  @Test
  public void testNoMatch() {
    RemoveTracking<String> removeTracking =
        getRemoveTracking(this::nullTransform, (contentIds) -> assertThat(contentIds).hasSize(0));
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    protocolBuilder.addFeature(idGenerators.createFeatureContentId(0), rootContentId);
    List<PayloadWithId> payloads = protocolBuilder.buildAsPayloadWithId();
    for (PayloadWithId payload : payloads) {
      assertThat(payload.payload.hasStreamFeature()).isTrue();
      removeTracking.filterStreamFeature(payload.payload.getStreamFeature());
    }
    removeTracking.triggerConsumerUpdate();
  }

  private RemoveTracking<String> getRemoveTracking(
      Function<StreamFeature, String> transformer, Consumer<List<String>> consumer) {
    return new RemoveTracking<>(transformer, consumer);
  }

  @SuppressWarnings("unused")
  private String nullTransform(StreamFeature streamFeature) {
    return null;
  }

  private String simpleTransform(StreamFeature streamFeature) {
    return streamFeature.getContentId();
  }
}
