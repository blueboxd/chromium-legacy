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

import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.ui.stream.StreamStructureProto.OfflineMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;

/** Metadata for content. */
public final class ContentMetadata {

  static final long UNKNOWN_TIME_PUBLISHED = -1L;

  private static final String TAG = "ContentMetadata";

  private final String url;
  private final String title;
  private final long timePublished;
  /*@Nullable*/ private final String imageUrl;
  /*@Nullable*/ private final String publisher;
  /*@Nullable*/ private final String faviconUrl;
  /*@Nullable*/ private final String snippet;

  /*@Nullable*/
  public static ContentMetadata maybeCreateContentMetadata(
      OfflineMetadata offlineMetadata, RepresentationData representationData) {
    if (!representationData.hasUri()) {
      Logger.w(TAG, "Can't build ContentMetadata with no URL");
      return null;
    }

    if (!offlineMetadata.hasTitle()) {
      Logger.w(TAG, "Can't build ContentMetadata with no title");
      return null;
    }

    String imageUrl = offlineMetadata.hasImageUrl() ? offlineMetadata.getImageUrl() : null;
    String publisher = offlineMetadata.hasPublisher() ? offlineMetadata.getPublisher() : null;
    String faviconUrl = offlineMetadata.hasFaviconUrl() ? offlineMetadata.getFaviconUrl() : null;
    String snippet = offlineMetadata.hasSnippet() ? offlineMetadata.getSnippet() : null;
    long publishedTimeSeconds =
        representationData.hasPublishedTimeSeconds()
            ? representationData.getPublishedTimeSeconds()
            : UNKNOWN_TIME_PUBLISHED;

    return new ContentMetadata(
        representationData.getUri(),
        offlineMetadata.getTitle(),
        publishedTimeSeconds,
        imageUrl,
        publisher,
        faviconUrl,
        snippet);
  }

  public ContentMetadata(
      String url,
      String title,
      long timePublished,
      /*@Nullable*/ String imageUrl,
      /*@Nullable*/ String publisher,
      /*@Nullable*/ String faviconUrl,
      /*@Nullable*/ String snippet) {
    this.url = url;
    this.title = title;
    this.imageUrl = imageUrl;
    this.publisher = publisher;
    this.faviconUrl = faviconUrl;
    this.snippet = snippet;
    this.timePublished = timePublished;
  }

  public String getUrl() {
    return url;
  }

  /** Title for the content. */
  public String getTitle() {
    return title;
  }

  /*@Nullable*/
  public String getImageUrl() {
    return imageUrl;
  }

  /** {@link String} representation of the publisher. */
  /*@Nullable*/
  public String getPublisher() {
    return publisher;
  }

  /**
   * Seconds of UTC time since the Unix Epoch 1970-01-01 T00:00:00Z or {@code
   * UNKNOWN_TIME_PUBLISHED} if unknown.
   */
  public long getTimePublished() {
    return timePublished;
  }

  /*@Nullable*/
  public String getFaviconUrl() {
    return faviconUrl;
  }

  /** A {@link String} that can be displayed that is part of the content, typically the start. */
  /*@Nullable*/
  public String getSnippet() {
    return snippet;
  }
}
