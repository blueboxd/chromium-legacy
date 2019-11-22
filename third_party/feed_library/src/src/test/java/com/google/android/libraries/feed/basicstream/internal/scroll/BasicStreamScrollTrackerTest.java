// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    @Mock
    private ScrollLogger logger;
    @Mock
    private ScrollObservable scrollObservable;
    private final FakeClock clock = new FakeClock();

    private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private BasicStreamScrollTracker scrollTracker;

    @Before
    public void setUp() {
        initMocks(this);
        scrollTracker =
                new BasicStreamScrollTracker(mainThreadRunner, logger, clock, scrollObservable);
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
