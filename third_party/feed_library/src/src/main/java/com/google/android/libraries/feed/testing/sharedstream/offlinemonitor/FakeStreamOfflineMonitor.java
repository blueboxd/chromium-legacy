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

package com.google.android.libraries.feed.testing.sharedstream.offlinemonitor;

import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.testing.host.offlineindicator.FakeOfflineIndicatorApi;

/** Fake used for tests using the {@link StreamOfflineMonitor}. */
public class FakeStreamOfflineMonitor extends StreamOfflineMonitor {

  private final FakeOfflineIndicatorApi fakeIndicatorApi;
  private boolean offlineStatusRequested = false;

  /** Creates a {@link FakeStreamOfflineMonitor} with the given {@link FakeOfflineIndicatorApi}. */
  public static FakeStreamOfflineMonitor createWithOfflineIndicatorApi(
      FakeOfflineIndicatorApi offlineIndicatorApi) {
    return new FakeStreamOfflineMonitor(offlineIndicatorApi);
  }

  /** Creates a {@link FakeStreamOfflineMonitor} with a default {@link FakeOfflineIndicatorApi}. */
  public static FakeStreamOfflineMonitor create() {
    return new FakeStreamOfflineMonitor(FakeOfflineIndicatorApi.createWithNoOfflineUrls());
  }

  private FakeStreamOfflineMonitor(FakeOfflineIndicatorApi offlineIndicatorApi) {
    super(offlineIndicatorApi);
    this.fakeIndicatorApi = offlineIndicatorApi;
  }

  /** Sets the offline status for the given {@code url}. */
  public void setOfflineStatus(String url, boolean isAvailable) {
    // Check if the url is available to marks the url as something to request from the
    // OfflineIndicatorApi.
    isAvailableOffline(url);

    // Sets the status of the url with the api, which is the source of truth.
    fakeIndicatorApi.setOfflineStatus(url, isAvailable);

    // Triggers notification to any current listeners, namely the superclass of this fake.
    requestOfflineStatusForNewContent();
  }

  /** Returns the count of how many observers there are for offline status. */
  public int getOfflineStatusConsumerCount() {
    return offlineStatusConsumersMap.size();
  }

  @Override
  public void requestOfflineStatusForNewContent() {
    offlineStatusRequested = true;
    super.requestOfflineStatusForNewContent();
  }

  public boolean isOfflineStatusRequested() {
    return offlineStatusRequested;
  }
}
