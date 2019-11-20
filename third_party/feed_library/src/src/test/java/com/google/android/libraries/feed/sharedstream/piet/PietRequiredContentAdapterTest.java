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

package com.google.android.libraries.feed.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation.Operation;
import com.google.search.now.wire.feed.FeatureProto.Feature;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PietRequiredContentAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class PietRequiredContentAdapterTest {
  private static final ContentId CONTENT_ID_1 = ContentId.newBuilder().setId(1).build();
  private static final ContentId CONTENT_ID_2 = ContentId.newBuilder().setId(2).build();
  private static final Feature FEATURE =
      Feature.newBuilder()
          .setExtension(
              Content.contentExtension,
              Content.newBuilder()
                  .setExtension(
                      PietContent.pietContentExtension,
                      PietContent.newBuilder()
                          .addPietSharedStates(CONTENT_ID_1)
                          .addPietSharedStates(CONTENT_ID_2)
                          .build())
                  .build())
          .build();

  private final PietRequiredContentAdapter adapter = new PietRequiredContentAdapter();

  @Test
  public void testDetermineRequiredContentIds() {
    assertThat(
            adapter.determineRequiredContentIds(
                DataOperation.newBuilder()
                    .setOperation(Operation.UPDATE_OR_APPEND)
                    .setFeature(FEATURE)
                    .build()))
        .containsExactly(CONTENT_ID_1, CONTENT_ID_2);
  }

  @Test
  public void testDetermineRequiredContentIds_removeDoesNotRequireContent() {
    assertThat(
            adapter.determineRequiredContentIds(
                DataOperation.newBuilder()
                    .setOperation(Operation.REMOVE)
                    .setFeature(FEATURE)
                    .build()))
        .isEmpty();
  }

  @Test
  public void testDetermineRequiredContentIds_clearAllDoesNotRequireContent() {
    assertThat(
            adapter.determineRequiredContentIds(
                DataOperation.newBuilder()
                    .setOperation(Operation.CLEAR_ALL)
                    .setFeature(FEATURE)
                    .build()))
        .isEmpty();
  }

  @Test
  public void testDetermineRequiredContentIds_defaultInstanceDoesNotRequireContent() {
    assertThat(
            adapter.determineRequiredContentIds(
                DataOperation.newBuilder().setOperation(Operation.UPDATE_OR_APPEND).build()))
        .isEmpty();
  }
}
