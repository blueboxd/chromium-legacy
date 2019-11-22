// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.testing.FakeClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;

/** Tests for {@link ScrollTracker}. */
@RunWith(RobolectricTestRunner.class)
public final class ScrollTrackerTest {
    private static final long AGGREGATE_TIME_MS = 200L;
    private final FakeClock clock = new FakeClock();
    private ScrollTrackerForTest scrollTracker;
    private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.create(clock);

    @Before
    public void setUp() {
        initMocks(this);
        scrollTracker = new ScrollTrackerForTest(mainThreadRunner, clock);
    }

    @Test
    public void testSinglePositiveScroll_scrollWrongDirection() {
        scrollTracker.trackScroll(10, 0);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testNoScroll() {
        scrollTracker.trackScroll(0, 0);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testSinglePositiveScroll_notEnoughTime() {
        scrollTracker.trackScroll(0, 10);
        clock.advance(AGGREGATE_TIME_MS - 1);

        assertThat(scrollTracker.scrollAmounts).isEmpty();
    }

    @Test
    public void testSinglePositiveScroll() {
        scrollTracker.trackScroll(0, 10);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).containsExactly(10);
    }

    @Test
    public void testDoublePositiveScroll_singleTask() {
        scrollTracker.trackScroll(0, 10);
        scrollTracker.trackScroll(0, 5);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).containsExactly(15);
    }

    @Test
    public void testDoublePositiveScroll_doubleTask() {
        scrollTracker.trackScroll(0, 10);
        clock.advance(AGGREGATE_TIME_MS);
        scrollTracker.trackScroll(0, 5);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).containsExactly(10, 5).inOrder();
    }

    @Test
    public void testSwitchScroll() {
        scrollTracker.trackScroll(0, 10);
        clock.advance(1L);
        scrollTracker.trackScroll(0, -5);
        clock.advance(AGGREGATE_TIME_MS);

        assertThat(scrollTracker.scrollAmounts).containsExactly(10, -5).inOrder();
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
