// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.knowncontent;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.OfflineMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ContentMetadata}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentMetadataTest {
    private static final String TITLE = "title";
    private static final String SNIPPET = "snippet";
    private static final String FAVICON_URL = "favicon.com";
    private static final String IMAGE_URL = "image.com";
    private static final String PUBLISHER = "publisher";
    private static final String URL = "url.com";
    private static final long TIME_PUBLISHED = 6L;

    private static final RepresentationData REPRESENTATION_DATA =
            RepresentationData.newBuilder()
                    .setUri(URL)
                    .setPublishedTimeSeconds(TIME_PUBLISHED)
                    .build();
    private static final RepresentationData REPRESENTATION_DATA_NO_URL =
            REPRESENTATION_DATA.toBuilder().clearUri().build();
    private static final RepresentationData REPRESENTATION_DATA_NO_TIME_PUBLISHED =
            REPRESENTATION_DATA.toBuilder().clearPublishedTimeSeconds().build();

    private static final OfflineMetadata OFFLINE_METADATA = OfflineMetadata.newBuilder()
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
        assertThat(ContentMetadata.maybeCreateContentMetadata(
                           OFFLINE_METADATA, REPRESENTATION_DATA_NO_URL))
                .isNull();
    }

    @Test
    public void testMaybeCreate_noTitle() {
        assertThat(ContentMetadata.maybeCreateContentMetadata(
                           OFFLINE_METADATA_NO_TITLE, REPRESENTATION_DATA))
                .isNull();
    }

    @Test
    public void testMaybeCreate_noTimePublished() {
        assertThat(ContentMetadata
                           .maybeCreateContentMetadata(
                                   OFFLINE_METADATA, REPRESENTATION_DATA_NO_TIME_PUBLISHED)
                           .getTimePublished())
                .isEqualTo(ContentMetadata.UNKNOWN_TIME_PUBLISHED);
    }
}
