// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.api.internal.knowncontent;

import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import java.util.List;

/** Allows the feed libraries to request and subscribe to information about the Feed's content. */
public interface FeedKnownContent extends KnownContent {

  /**
   * Gets listener that notifies all added listeners of {@link
   * KnownContent.Listener#onContentRemoved(List)} or {@link
   * KnownContent.Listener#onNewContentReceived(boolean, long)}.
   *
   * <p>Note: This method is internal to the Feed. It provides a {@link Listener} that, when
   * notified, will propagate the notification to the host.
   */
  KnownContent.Listener getKnownContentHostNotifier();
}
