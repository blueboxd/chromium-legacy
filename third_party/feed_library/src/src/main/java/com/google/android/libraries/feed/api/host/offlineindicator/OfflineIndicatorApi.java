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

package com.google.android.libraries.feed.api.host.offlineindicator;

import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.List;

/** Api to allow the Feed to get information about offline availability status of content. */
public interface OfflineIndicatorApi {

  /**
   * Requests information on the offline status of content shown in the Feed.
   *
   * @param urlsToRetrieve list of urls we want to know about.
   * @param urlListConsumer subset of {@code urlsToRetrieve} which are available offline.
   */
  void getOfflineStatus(List<String> urlsToRetrieve, Consumer<List<String>> urlListConsumer);

  /** Adds a listener for changes to the offline availability of content. */
  void addOfflineStatusListener(OfflineStatusListener offlineStatusListener);

  /** Removes listener for changes the offline availability of content. */
  void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener);

  /** Listener for changes in the offline availability of content. */
  interface OfflineStatusListener {
    void updateOfflineStatus(String url, boolean availableOffline);
  }
}
