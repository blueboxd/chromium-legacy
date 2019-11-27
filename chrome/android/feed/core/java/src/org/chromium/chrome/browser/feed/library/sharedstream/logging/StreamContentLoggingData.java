// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.ClientBasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;

import java.util.Objects;

/** Implementation of {@link ContentLoggingData} to capture content data when logging events. */
public class StreamContentLoggingData implements ContentLoggingData {
    private final int mPositionInStream;
    private final long mPublishedTimeSeconds;
    private final long mTimeContentBecameAvailable;
    private final float mScore;
    private final String mRepresentationUri;
    private final boolean mIsAvailableOffline;

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
        this.mPositionInStream = positionInStream;
        this.mPublishedTimeSeconds = publishedTimeSeconds;
        this.mTimeContentBecameAvailable = timeContentBecameAvailable;
        this.mScore = score;
        this.mRepresentationUri = representationUri;
        this.mIsAvailableOffline = isAvailableOffline;
    }

    @Override
    public int getPositionInStream() {
        return mPositionInStream;
    }

    @Override
    public long getPublishedTimeSeconds() {
        return mPublishedTimeSeconds;
    }

    @Override
    public long getTimeContentBecameAvailable() {
        return mTimeContentBecameAvailable;
    }

    @Override
    public float getScore() {
        return mScore;
    }

    @Override
    public String getRepresentationUri() {
        return mRepresentationUri;
    }

    @Override
    public boolean isAvailableOffline() {
        return mIsAvailableOffline;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mPositionInStream, mPublishedTimeSeconds, mTimeContentBecameAvailable,
                mScore, mRepresentationUri, mIsAvailableOffline);
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

        if (mPositionInStream != that.mPositionInStream) {
            return false;
        }
        if (mPublishedTimeSeconds != that.mPublishedTimeSeconds) {
            return false;
        }
        if (mTimeContentBecameAvailable != that.mTimeContentBecameAvailable) {
            return false;
        }
        if (Float.compare(that.mScore, mScore) != 0) {
            return false;
        }
        if (mIsAvailableOffline != that.mIsAvailableOffline) {
            return false;
        }

        return Objects.equals(mRepresentationUri, that.mRepresentationUri);
    }

    @Override
    public String toString() {
        return "StreamContentLoggingData{"
                + "positionInStream=" + mPositionInStream
                + ", publishedTimeSeconds=" + mPublishedTimeSeconds
                + ", timeContentBecameAvailable=" + mTimeContentBecameAvailable
                + ", score=" + mScore + ", representationUri='" + mRepresentationUri + '\'' + '}';
    }

    /**
     * Returns a {@link StreamContentLoggingData} instance with {@code this} as a basis, but with
     * the given offline status.
     *
     * <p>Will not create a new instance if unneeded.
     */
    public StreamContentLoggingData createWithOfflineStatus(boolean offlineStatus) {
        if (offlineStatus == this.mIsAvailableOffline) {
            return this;
        }

        return new StreamContentLoggingData(mPositionInStream, mPublishedTimeSeconds,
                mTimeContentBecameAvailable, mScore, mRepresentationUri, offlineStatus);
    }
}
