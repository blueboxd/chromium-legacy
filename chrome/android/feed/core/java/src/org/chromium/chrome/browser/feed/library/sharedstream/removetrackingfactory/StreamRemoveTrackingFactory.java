// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.removetrackingfactory;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentRemoval;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.RemoveTracking;

/** {@link RemoveTrackingFactory} to notify host of removed content. */
public class StreamRemoveTrackingFactory implements RemoveTrackingFactory<ContentRemoval> {
    private final ModelProvider mModelProvider;
    private final FeedKnownContent mFeedKnownContent;

    public StreamRemoveTrackingFactory(
            ModelProvider modelProvider, FeedKnownContent feedKnownContent) {
        this.mModelProvider = modelProvider;
        this.mFeedKnownContent = feedKnownContent;
    }

    @Nullable
    @Override
    public RemoveTracking<ContentRemoval> create(MutationContext mutationContext) {
        String requestingSessionId = mutationContext.getRequestingSessionId();
        if (requestingSessionId == null) {
            return null;
        }

        // Only notify host on the StreamScope that requested the dismiss.
        if (!requestingSessionId.equals(mModelProvider.getSessionId())) {
            return null;
        }

        return new RemoveTracking<>(
                streamFeature
                -> {
                    if (!streamFeature.getContent().getRepresentationData().hasUri()) {
                        return null;
                    }

                    return new ContentRemoval(
                            streamFeature.getContent().getRepresentationData().getUri(),
                            mutationContext.isUserInitiated());
                },
                removedContent
                -> mFeedKnownContent.getKnownContentHostNotifier().onContentRemoved(
                        removedContent));
    }
}
