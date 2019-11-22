// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

import com.google.android.libraries.feed.api.host.logging.ContentLoggingData;
import com.google.search.now.feed.client.StreamDataProto.ClientBasicLoggingMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;

import java.util.Objects;

/** Implementation of {@link ContentLoggingData} to capture content data when logging events. */
public class StreamContentLoggingData implements ContentLoggingData {
    private final int positionInStream;
    private final long publishedTimeSeconds;
    private final long timeContentBecameAvailable;
    private final float score;
    private final String representationUri;
    private final boolean isAvailableOffline;

    public StreamContentLoggingData(int positionInStream, BasicLoggingMetadata basicLoggingMetadata,
            RepresentationData representationData, boolean availableOffline) {
        this(positionInStream, representationData.getPublishedTimeSeconds(),
                basicLoggingMetadata
                        .getExtension(ClientBasicLoggingMetadata.clientBasicLoggingMetadata)
                        .getAvailabilityTimeSeconds(),
                basicLoggingMetadata.getScore(), representationData.getUri(), availableOffline);
    }

    private StreamContentLoggingData(int positionInStream, long publishedTimeSeconds,
            long timeContentBecameAvailable, float score, String representationUri,
            boolean isAvailableOffline) {
        this.positionInStream = positionInStream;
        this.publishedTimeSeconds = publishedTimeSeconds;
        this.timeContentBecameAvailable = timeContentBecameAvailable;
        this.score = score;
        this.representationUri = representationUri;
        this.isAvailableOffline = isAvailableOffline;
    }

    @Override
    public int getPositionInStream() {
        return positionInStream;
    }

    @Override
    public long getPublishedTimeSeconds() {
        return publishedTimeSeconds;
    }

    @Override
    public long getTimeContentBecameAvailable() {
        return timeContentBecameAvailable;
    }

    @Override
    public float getScore() {
        return score;
    }

    @Override
    public String getRepresentationUri() {
        return representationUri;
    }

    @Override
    public boolean isAvailableOffline() {
        return isAvailableOffline;
    }

    @Override
    public int hashCode() {
        return Objects.hash(positionInStream, publishedTimeSeconds, timeContentBecameAvailable,
                score, representationUri, isAvailableOffline);
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof StreamContentLoggingData)) {
            return false;
        }

        StreamContentLoggingData that = (StreamContentLoggingData) o;

        if (positionInStream != that.positionInStream) {
            return false;
        }
        if (publishedTimeSeconds != that.publishedTimeSeconds) {
            return false;
        }
        if (timeContentBecameAvailable != that.timeContentBecameAvailable) {
            return false;
        }
        if (Float.compare(that.score, score) != 0) {
            return false;
        }
        if (isAvailableOffline != that.isAvailableOffline) {
            return false;
        }

        return Objects.equals(representationUri, that.representationUri);
    }

    @Override
    public String toString() {
        return "StreamContentLoggingData{"
                + "positionInStream=" + positionInStream
                + ", publishedTimeSeconds=" + publishedTimeSeconds
                + ", timeContentBecameAvailable=" + timeContentBecameAvailable + ", score=" + score
                + ", representationUri='" + representationUri + '\'' + '}';
    }

    /**
     * Returns a {@link StreamContentLoggingData} instance with {@code this} as a basis, but with
     * the given offline status.
     *
     * <p>Will not create a new instance if unneeded.
     */
    public StreamContentLoggingData createWithOfflineStatus(boolean offlineStatus) {
        if (offlineStatus == this.isAvailableOffline) {
            return this;
        }

        return new StreamContentLoggingData(positionInStream, publishedTimeSeconds,
                timeContentBecameAvailable, score, representationUri, offlineStatus);
    }
}
