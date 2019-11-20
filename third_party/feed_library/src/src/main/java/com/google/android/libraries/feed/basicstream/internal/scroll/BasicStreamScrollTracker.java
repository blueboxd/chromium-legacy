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

import android.view.View;
import com.google.android.libraries.feed.api.host.logging.ScrollType;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollLogger;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollTracker;

/** A @link{ScrollTracker} used by BasicStream */
public class BasicStreamScrollTracker extends ScrollTracker {
  private final ScrollLogger scrollLogger;
  private final ScrollObserver scrollObserver;
  private final ScrollObservable scrollObservable;

  public BasicStreamScrollTracker(
      MainThreadRunner mainThreadRunner,
      ScrollLogger scrollLogger,
      Clock clock,
      ScrollObservable scrollObservable) {
    super(mainThreadRunner, clock);
    this.scrollLogger = scrollLogger;
    this.scrollObservable = scrollObservable;
    this.scrollObserver = new BasicStreamScrollObserver();
    scrollObservable.addScrollObserver(scrollObserver);
  }

  @Override
  protected void onScrollEvent(int scrollAmount, long timestamp) {
    scrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
  }

  @Override
  public void onUnbind() {
    super.onUnbind();
    scrollObservable.removeScrollObserver(scrollObserver);
  }

  private class BasicStreamScrollObserver implements ScrollObserver {

    @Override
    public void onScrollStateChanged(View view, String featureId, int newState, long timestamp) {}

    @Override
    public void onScroll(View view, String featureId, int dx, int dy) {
      trackScroll(dx, dy);
    }
  }
}
