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

import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.List;

/** Allows the host to request and subscribe to information about the Feed's content. */
public interface KnownContent {
  /**
   * Async call to get the list of all content that is known about by the Feed. The list will be in
   * the same order that the content will appear.
   *
   * <p>Note: This method can be expensive. As such it should not be called at latency critical
   * moments, namely during startup.
   */
  void getKnownContent(Consumer<List<ContentMetadata>> knownContentConsumer);

  /** Adds listener for new content. */
  void addListener(Listener listener);

  /** Removes listener for new content. */
  void removeListener(Listener listener);

  /** Listener for when content is added or removed. */
  interface Listener {
    /**
     * Called when content is removed.
     *
     * @param contentRemoved {@link List} of removed content.
     */
    void onContentRemoved(List<ContentRemoval> contentRemoved);

    /**
     * Notifies host that new content has been received.
     *
     * @param isNewRefresh {@code true} if the content is from a new refresh, {@code false}
     *     otherwise, such as from a pagination request.
     * @param contentCreationDateTimeMs the date/time of when the content was added in milliseconds.
     */
    void onNewContentReceived(boolean isNewRefresh, long contentCreationDateTimeMs);
  }
}
