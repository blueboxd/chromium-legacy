// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.knowncontent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.OfflineMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;

/** Metadata for content. */
public final class ContentMetadata {
    static final long UNKNOWN_TIME_PUBLISHED = -1L;

    private static final String TAG = "ContentMetadata";

    private final String mUrl;
    private final String mTitle;
    private final long mTimePublished;
    @Nullable
    private final String mImageUrl;
    @Nullable
    private final String mPublisher;
    @Nullable
    private final String mFaviconUrl;
    @Nullable
    private final String mSnippet;

    @Nullable
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
        String faviconUrl =
                offlineMetadata.hasFaviconUrl() ? offlineMetadata.getFaviconUrl() : null;
        String snippet = offlineMetadata.hasSnippet() ? offlineMetadata.getSnippet() : null;
        long publishedTimeSeconds = representationData.hasPublishedTimeSeconds()
                ? representationData.getPublishedTimeSeconds()
                : UNKNOWN_TIME_PUBLISHED;

        return new ContentMetadata(representationData.getUri(), offlineMetadata.getTitle(),
                publishedTimeSeconds, imageUrl, publisher, faviconUrl, snippet);
    }

    public ContentMetadata(String url, String title, long timePublished, @Nullable String imageUrl,
            @Nullable String publisher, @Nullable String faviconUrl, @Nullable String snippet) {
        this.mUrl = url;
        this.mTitle = title;
        this.mImageUrl = imageUrl;
        this.mPublisher = publisher;
        this.mFaviconUrl = faviconUrl;
        this.mSnippet = snippet;
        this.mTimePublished = timePublished;
    }

    public String getUrl() {
        return mUrl;
    }

    /** Title for the content. */
    public String getTitle() {
        return mTitle;
    }

    @Nullable
    public String getImageUrl() {
        return mImageUrl;
    }

    /** {@link String} representation of the publisher. */
    @Nullable
    public String getPublisher() {
        return mPublisher;
    }

    /**
     * Seconds of UTC time since the Unix Epoch 1970-01-01 T00:00:00Z or {@code
     * UNKNOWN_TIME_PUBLISHED} if unknown.
     */
    public long getTimePublished() {
        return mTimePublished;
    }

    @Nullable
    public String getFaviconUrl() {
        return mFaviconUrl;
    }

    /** A {@link String} that can be displayed that is part of the content, typically the start. */
    @Nullable
    public String getSnippet() {
        return mSnippet;
    }
}
