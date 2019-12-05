// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/** Tests for {@link ScrollTracker}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ScrollTrackerTest {
    private static final long AGGREGATE_TIME_MS = 200L;
    private final FakeClock mClock = new FakeClock();
    private ScrollTrackerForTest mScrollTracker;
    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.create(mClock);

    @Before
    public void setUp() {
        initMocks(this);
        mScrollTracker = new ScrollTrackerForTest(mMainThreadRunner, mClock);
    }

    @Test
    public void testSinglePositiveScroll_scrollWrongDirection() {
        mScrollTracker.trackScroll(10, 0);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testNoScroll() {
        mScrollTracker.trackScroll(0, 0);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testSinglePositiveScroll_notEnoughTime() {
        mScrollTracker.trackScroll(0, 10);
        mClock.advance(AGGREGATE_TIME_MS - 1);

        assertThat(mScrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testSinglePositiveScroll() {
        mScrollTracker.trackScroll(0, 10);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).containsExactly(10);
    }

    @Test
    public void testDoublePositiveScroll_singleTask() {
        mScrollTracker.trackScroll(0, 10);
        mScrollTracker.trackScroll(0, 5);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).containsExactly(15);
    }

    @Test
    public void testDoublePositiveScroll_doubleTask() {
        mScrollTracker.trackScroll(0, 10);
        mClock.advance(AGGREGATE_TIME_MS);
        mScrollTracker.trackScroll(0, 5);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).containsExactly(10, 5).inOrder();
    }

    @Test
    public void testSwitchScroll() {
        mScrollTracker.trackScroll(0, 10);
        mClock.advance(1L);
        mScrollTracker.trackScroll(0, -5);
        mClock.advance(AGGREGATE_TIME_MS);

        assertThat(mScrollTracker.scrollAmounts).containsExactly(10, -5).inOrder();
    }

    static class ScrollTrackerForTest extends ScrollTracker {
        public ArrayList<Integer> scrollAmounts;

        ScrollTrackerForTest(FakeMainThreadRunner mainThreadRunner, FakeClock clock) {
            super(mainThreadRunner, clock);
            scrollAmounts = new ArrayList<>();
        }

        @Override
        protected void onScrollEvent(int scrollAmount, long timestamp) {
            scrollAmounts.add(scrollAmount);
        }
    }
}
