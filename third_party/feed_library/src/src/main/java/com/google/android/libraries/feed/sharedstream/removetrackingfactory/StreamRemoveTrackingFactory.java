// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.removetrackingfactory;

import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.RemoveTracking;

/** {@link RemoveTrackingFactory} to notify host of removed content. */
public class StreamRemoveTrackingFactory implements RemoveTrackingFactory<ContentRemoval> {
    private final ModelProvider mModelProvider;
    private final FeedKnownContent mFeedKnownContent;

    public StreamRemoveTrackingFactory(
            ModelProvider modelProvider, FeedKnownContent feedKnownContent) {
        this.mModelProvider = modelProvider;
        this.mFeedKnownContent = feedKnownContent;
    }

    /*@Nullable*/
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
