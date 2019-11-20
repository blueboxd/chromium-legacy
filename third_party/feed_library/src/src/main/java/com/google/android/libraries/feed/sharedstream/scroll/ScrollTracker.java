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
package com.google.android.libraries.feed.sharedstream.scroll;

import com.google.android.libraries.feed.common.concurrent.CancelableTask;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.time.Clock;

/**
 * Helper which is able to track a Scroll and aggregate them before sending the scroll events
 * events.
 */
public abstract class ScrollTracker {
  private static final String TAG = "ScrollTracker";
  // onScroll events are very noisy, so we collate them together to avoid over-reporting scrolls.
  private static final long SCROLL_EVENT_COLLATE_MILLIS = 200L;

  private final MainThreadRunner mainThreadRunner;
  private final Clock clock;

  /*@Nullable*/ protected ScrollNotifier scrollNotifier;
  /*@Nullable*/ protected CancelableTask task;

  public ScrollTracker(MainThreadRunner mainThreadRunner, Clock clock) {
    this.mainThreadRunner = mainThreadRunner;
    this.clock = clock;
  }

  public void onUnbind() {
    ScrollTracker.ScrollNotifier localScrollNotifier = scrollNotifier;
    if (localScrollNotifier != null) {
      if (task != null) {
        task.cancel();
      }
      localScrollNotifier.run();
    }
  }

  protected void trackScroll(int dx, int dy) {
    boolean positiveScroll;
    int amount;
    if (dy == 0) {
      return;
    }
    positiveScroll = dy > 0;
    amount = dy;

    int previousTotalScroll = 0;
    ScrollNotifier previousScrollNotifier = scrollNotifier;
    if (previousScrollNotifier != null && !(task != null && task.canceled())) {
      if (previousScrollNotifier.positiveScroll == positiveScroll) {
        // Same direction, so merge the existing scroll with the new one.
        previousTotalScroll = previousScrollNotifier.scrollAmount;
        if (task != null) {
          task.cancel();
        }
      }
    }

    amount += previousTotalScroll;
    scrollNotifier = new ScrollNotifier(positiveScroll, amount, clock.currentTimeMillis());
    task = mainThreadRunner.executeWithDelay(TAG, scrollNotifier, SCROLL_EVENT_COLLATE_MILLIS);
  }

  protected abstract void onScrollEvent(int scrollAmount, long timestamp);

  private class ScrollNotifier implements Runnable {
    final boolean positiveScroll;
    final int scrollAmount;
    final long timestamp;

    public ScrollNotifier(boolean positiveScroll, int scrollAmount, long timestamp) {
      this.positiveScroll = positiveScroll;
      this.scrollAmount = scrollAmount;
      this.timestamp = timestamp;
    }

    @Override
    public void run() {
      onScrollEvent(scrollAmount, timestamp);
      if (scrollNotifier == this) {
        scrollNotifier = null;
        if (task != null) {
          task.cancel();
        }
      }
    }
  }
}
