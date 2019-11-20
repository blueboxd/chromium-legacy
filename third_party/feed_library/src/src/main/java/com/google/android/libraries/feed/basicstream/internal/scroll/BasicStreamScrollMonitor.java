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

package com.google.android.libraries.feed.basicstream.internal.scroll;

import android.support.v7.widget.RecyclerView;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Monitors and dispatches scroll events to the registered listeners.
 *
 * <p>Note: This is intentionally unscoped, because we want features to be able to manage their
 * scroll locally, instead of having a singleton ScrollMonitor.
 */
public class BasicStreamScrollMonitor extends RecyclerView.OnScrollListener
    implements ScrollObservable {

  private final Set<ScrollObserver> scrollObservers;
  private final Clock clock;

  private int currentScrollState = RecyclerView.SCROLL_STATE_IDLE;

  public BasicStreamScrollMonitor(Clock clock) {
    this.clock = clock;
    scrollObservers = Collections.newSetFromMap(new ConcurrentHashMap<>());
  }

  @Override
  public void addScrollObserver(ScrollObserver scrollObserver) {
    scrollObservers.add(scrollObserver);
  }

  @Override
  public void removeScrollObserver(ScrollObserver scrollObserver) {
    scrollObservers.remove(scrollObserver);
  }

  @Override
  public int getCurrentScrollState() {
    return currentScrollState;
  }

  public int getObserverCount() {
    return scrollObservers.size();
  }

  /**
   * Notify the monitor of a scroll state change event that should be dispatched to the observers.
   */
  @Override
  public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
    currentScrollState = newState;
    for (ScrollObserver observer : scrollObservers) {
      observer.onScrollStateChanged(recyclerView, "", newState, clock.currentTimeMillis());
    }
  }

  /** Notify the monitor of a scroll event that should be dispatched to its observers. */
  @Override
  public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
    for (ScrollObserver observer : scrollObservers) {
      observer.onScroll(recyclerView, "", dx, dy);
    }
  }
}
