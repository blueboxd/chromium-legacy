// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.time.testing;

import static com.google.common.truth.Truth.assertThat;

import org.chromium.chrome.browser.feed.library.common.feedobservable.ObservableNotifier;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock.ClockUpdateListener;

/** Fake implementation of {@link Clock} for testing. */
public class FakeClock extends ObservableNotifier<ClockUpdateListener> implements Clock {
    private static final String TAG = "FakeClock";

    private long mCurrentTime;
    private long mElapsedRealtimeMs;

    public FakeClock() {
        this(0, 0);
    }

    public FakeClock(long currentTime, long elapsedRealtime) {
        this.mCurrentTime = currentTime;
        mElapsedRealtimeMs = elapsedRealtime;
    }

    /**
     * Sets the current time and elapsed real time to the same value. Returns {@code this} for
     * convenience.
     */
    public FakeClock set(long millis) {
        return set(millis, millis);
    }

    /**
     * Sets the current time and elapsed real time individually. Returns {@code this} for
     * convenience.
     */
    public FakeClock set(long currentTime, long elapsedRealtime) {
        this.mCurrentTime = currentTime;
        mElapsedRealtimeMs = elapsedRealtime;
        onClockUpdated(currentTime, elapsedRealtime);
        return this;
    }

    public void tick() {
        advance(1);
    }

    public void advance(long millis) {
        assertThat(millis).isAtLeast(0L);
        mCurrentTime += millis;
        mElapsedRealtimeMs += millis;

        Logger.i(TAG, "Advancing clock to %d", mCurrentTime);
        onClockUpdated(mCurrentTime, mElapsedRealtimeMs);
    }

    /**
     * Advance the clock forward to the specified time. This will fail if {@link currentTimeMillis}
     * is not the current time or in the future.
     */
    public void advanceTo(long currentTimeMillis) {
        advance(currentTimeMillis - currentTimeMillis());
    }

    @Override
    public long currentTimeMillis() {
        return mCurrentTime;
    }

    @Override
    public long elapsedRealtime() {
        return mElapsedRealtimeMs;
    }

    @Override
    public long uptimeMillis() {
        return mElapsedRealtimeMs;
    }

    private void onClockUpdated(long currentTime, long elapsedRealtime) {
        notifyListeners(listener -> listener.onClockUpdated(currentTime, elapsedRealtime));
    }

    /** Listener for when the clock time is updated. */
    public interface ClockUpdateListener {
        void onClockUpdated(long newCurrentTime, long newElapsedRealtime);
    }
}
