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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.ScrollType;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollLogger;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link BasicStreamScrollTracker}. */
@RunWith(RobolectricTestRunner.class)
public class BasicStreamScrollTrackerTest {

  @Mock private ScrollLogger logger;
  @Mock private ScrollObservable scrollObservable;
  private final FakeClock clock = new FakeClock();

  private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
  private BasicStreamScrollTracker scrollTracker;

  @Before
  public void setUp() {
    initMocks(this);
    scrollTracker = new BasicStreamScrollTracker(mainThreadRunner, logger, clock, scrollObservable);
    verify(scrollObservable).addScrollObserver(any(ScrollObserver.class));
  }

  @Test
  public void onUnbind_removedScrollListener() {
    scrollTracker.onUnbind();
    verify(scrollObservable).removeScrollObserver(any(ScrollObserver.class));
  }

  @Test
  public void onScrollEvent() {
    int scrollAmount = 10;
    long timestamp = 10L;
    scrollTracker.onScrollEvent(scrollAmount, timestamp);
    verify(logger).handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
  }
}
