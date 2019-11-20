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

package com.google.android.libraries.feed.api.client.knowncontent;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.ui.stream.StreamStructureProto.OfflineMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ContentMetadata}. */
@RunWith(RobolectricTestRunner.class)
public class ContentMetadataTest {

  private static final String TITLE = "title";
  private static final String SNIPPET = "snippet";
  private static final String FAVICON_URL = "favicon.com";
  private static final String IMAGE_URL = "image.com";
  private static final String PUBLISHER = "publisher";
  private static final String URL = "url.com";
  private static final long TIME_PUBLISHED = 6L;

  private static final RepresentationData REPRESENTATION_DATA =
      RepresentationData.newBuilder().setUri(URL).setPublishedTimeSeconds(TIME_PUBLISHED).build();
  private static final RepresentationData REPRESENTATION_DATA_NO_URL =
      REPRESENTATION_DATA.toBuilder().clearUri().build();
  private static final RepresentationData REPRESENTATION_DATA_NO_TIME_PUBLISHED =
      REPRESENTATION_DATA.toBuilder().clearPublishedTimeSeconds().build();

  private static final OfflineMetadata OFFLINE_METADATA =
      OfflineMetadata.newBuilder()
          .setFaviconUrl(FAVICON_URL)
          .setImageUrl(IMAGE_URL)
          .setPublisher(PUBLISHER)
          .setSnippet(SNIPPET)
          .setTitle(TITLE)
          .build();
  private static final OfflineMetadata OFFLINE_METADATA_NO_TITLE =
      OFFLINE_METADATA.toBuilder().clearTitle().build();

  @Test
  public void testMaybeCreate() {
    ContentMetadata created =
        ContentMetadata.maybeCreateContentMetadata(OFFLINE_METADATA, REPRESENTATION_DATA);

    assertThat(created.getTitle()).isEqualTo(TITLE);
    assertThat(created.getSnippet()).isEqualTo(SNIPPET);
    assertThat(created.getFaviconUrl()).isEqualTo(FAVICON_URL);
    assertThat(created.getImageUrl()).isEqualTo(IMAGE_URL);
    assertThat(created.getPublisher()).isEqualTo(PUBLISHER);
    assertThat(created.getUrl()).isEqualTo(URL);
    assertThat(created.getTimePublished()).isEqualTo(TIME_PUBLISHED);
  }

  @Test
  public void testMaybeCreate_noUrl() {
    assertThat(
            ContentMetadata.maybeCreateContentMetadata(
                OFFLINE_METADATA, REPRESENTATION_DATA_NO_URL))
        .isNull();
  }

  @Test
  public void testMaybeCreate_noTitle() {
    assertThat(
            ContentMetadata.maybeCreateContentMetadata(
                OFFLINE_METADATA_NO_TITLE, REPRESENTATION_DATA))
        .isNull();
  }

  @Test
  public void testMaybeCreate_noTimePublished() {
    assertThat(
            ContentMetadata.maybeCreateContentMetadata(
                    OFFLINE_METADATA, REPRESENTATION_DATA_NO_TIME_PUBLISHED)
                .getTimePublished())
        .isEqualTo(ContentMetadata.UNKNOWN_TIME_PUBLISHED);
  }
}
