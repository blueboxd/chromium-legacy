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

package com.google.android.libraries.feed.testing.host.offlineindicator;

import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Fake used for tests using the {@link OfflineIndicatorApi}. */
public class FakeOfflineIndicatorApi implements OfflineIndicatorApi {

  private final Set<String> offlineUrls;
  private final Set<OfflineStatusListener> listeners;

  private FakeOfflineIndicatorApi(String[] urls) {
    offlineUrls = new HashSet<>(Arrays.asList(urls));
    listeners = new HashSet<>();
  }

  public static FakeOfflineIndicatorApi createWithOfflineUrls(String... urls) {
    return new FakeOfflineIndicatorApi(urls);
  }

  public static FakeOfflineIndicatorApi createWithNoOfflineUrls() {
    return createWithOfflineUrls();
  }

  @Override
  public void getOfflineStatus(
      List<String> urlsToRetrieve, Consumer<List<String>> urlListConsumer) {
    HashSet<String> copiedHashSet = new HashSet<>(offlineUrls);
    copiedHashSet.retainAll(urlsToRetrieve);

    urlListConsumer.accept(new ArrayList<>(copiedHashSet));
  }

  @Override
  public void addOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
    listeners.add(offlineStatusListener);
  }

  @Override
  public void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
    listeners.remove(offlineStatusListener);
  }

  /**
   * Sets the offline status of the given {@code url} to the given value and notifies any observers.
   */
  public void setOfflineStatus(String url, boolean isAvailable) {
    boolean statusChanged = isAvailable ? offlineUrls.add(url) : offlineUrls.remove(url);

    if (!statusChanged) {
      return;
    }

    for (OfflineStatusListener listener : listeners) {
      listener.updateOfflineStatus(url, isAvailable);
    }
  }
}
