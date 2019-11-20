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

package com.google.android.libraries.feed.sharedstream.removetrackingfactory;

import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.RemoveTracking;

/** {@link RemoveTrackingFactory} to notify host of removed content. */
public class StreamRemoveTrackingFactory implements RemoveTrackingFactory<ContentRemoval> {

  private final ModelProvider modelProvider;
  private final FeedKnownContent feedKnownContent;

  public StreamRemoveTrackingFactory(
      ModelProvider modelProvider, FeedKnownContent feedKnownContent) {
    this.modelProvider = modelProvider;
    this.feedKnownContent = feedKnownContent;
  }

  /*@Nullable*/
  @Override
  public RemoveTracking<ContentRemoval> create(MutationContext mutationContext) {
    String requestingSessionId = mutationContext.getRequestingSessionId();
    if (requestingSessionId == null) {
      return null;
    }

    // Only notify host on the StreamScope that requested the dismiss.
    if (!requestingSessionId.equals(modelProvider.getSessionId())) {
      return null;
    }

    return new RemoveTracking<>(
        streamFeature -> {
          if (!streamFeature.getContent().getRepresentationData().hasUri()) {
            return null;
          }

          return new ContentRemoval(
              streamFeature.getContent().getRepresentationData().getUri(),
              mutationContext.isUserInitiated());
        },
        removedContent ->
            feedKnownContent.getKnownContentHostNotifier().onContentRemoved(removedContent));
  }
}
