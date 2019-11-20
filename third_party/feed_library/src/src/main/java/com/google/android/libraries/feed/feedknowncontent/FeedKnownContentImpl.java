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

package com.google.android.libraries.feed.feedknowncontent;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Default implementation of the {@link KnownContent}. */
public final class FeedKnownContentImpl implements FeedKnownContent {

  private static final String TAG = "FeedKnownContentImpl";
  private final FeedSessionManager feedSessionManager;
  private final Set<KnownContent.Listener> listeners = new HashSet<>();
  private final MainThreadRunner mainThreadRunner;
  private final ThreadUtils threadUtils;
  private final KnownContent.Listener listener;

  @SuppressWarnings("nullness:method.invocation.invalid")
  public FeedKnownContentImpl(
      FeedSessionManager feedSessionManager,
      MainThreadRunner mainThreadRunner,
      ThreadUtils threadUtils) {
    this.feedSessionManager = feedSessionManager;
    this.mainThreadRunner = mainThreadRunner;
    this.threadUtils = threadUtils;

    this.listener =
        new KnownContent.Listener() {
          @Override
          public void onContentRemoved(List<ContentRemoval> contentRemoved) {
            runOnMainThread(
                TAG + " onContentRemoved",
                () -> {
                  for (KnownContent.Listener knownContentListener : listeners) {
                    knownContentListener.onContentRemoved(contentRemoved);
                  }
                });
          }

          @Override
          public void onNewContentReceived(boolean isNewRefresh, long contentCreationDateTimeMs) {
            runOnMainThread(
                TAG + " onNewContentReceived",
                () -> {
                  for (KnownContent.Listener knownContentListener : listeners) {
                    knownContentListener.onNewContentReceived(
                        isNewRefresh, contentCreationDateTimeMs);
                  }
                });
          }
        };

    feedSessionManager.setKnownContentListener(this.listener);
  }

  @Override
  public void getKnownContent(Consumer<List<ContentMetadata>> knownContentConsumer) {
    feedSessionManager.getStreamFeaturesFromHead(
        streamPayload -> {
          if (!streamPayload.getStreamFeature().hasContent()) {
            return null;
          }

          Content content = streamPayload.getStreamFeature().getContent();

          return ContentMetadata.maybeCreateContentMetadata(
              content.getOfflineMetadata(), content.getRepresentationData());
        },
        (Result<List<ContentMetadata>> result) -> {
          runOnMainThread(
              TAG + " getKnownContentAccept",
              () -> {
                if (!result.isSuccessful()) {
                  Logger.e(TAG, "Can't inform on known content due to internal feed error.");
                  return;
                }

                knownContentConsumer.accept(result.getValue());
              });
        });
  }

  @Override
  public void addListener(KnownContent.Listener listener) {
    listeners.add(listener);
  }

  @Override
  public void removeListener(KnownContent.Listener listener) {
    listeners.remove(listener);
  }

  @Override
  public KnownContent.Listener getKnownContentHostNotifier() {
    return listener;
  }

  private void runOnMainThread(String name, Runnable runnable) {
    if (threadUtils.isMainThread()) {
      runnable.run();
      return;
    }

    mainThreadRunner.execute(name, runnable);
  }
}
