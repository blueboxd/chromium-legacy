// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.scroll;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollLogger;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link BasicStreamScrollTracker}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicStreamScrollTrackerTest {
    @Mock
    private ScrollLogger mLogger;
    @Mock
    private ScrollObservable mScrollObservable;
    private final FakeClock mClock = new FakeClock();

    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private BasicStreamScrollTracker mScrollTracker;

    @Before
    public void setUp() {
        initMocks(this);
        mScrollTracker =
                new BasicStreamScrollTracker(mMainThreadRunner, mLogger, mClock, mScrollObservable);
        verify(mScrollObservable).addScrollObserver(any(ScrollObserver.class));
    }

    @Test
    public void onUnbind_removedScrollListener() {
        mScrollTracker.onUnbind();
        verify(mScrollObservable).removeScrollObserver(any(ScrollObserver.class));
    }

    @Test
    public void onScrollEvent() {
        int scrollAmount = 10;
        long timestamp = 10L;
        mScrollTracker.onScrollEvent(scrollAmount, timestamp);
        verify(mLogger).handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }
}
