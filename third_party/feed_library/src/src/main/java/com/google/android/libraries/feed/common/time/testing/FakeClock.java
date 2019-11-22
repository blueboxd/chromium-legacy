// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.time.testing;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.common.feedobservable.ObservableNotifier;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.testing.FakeClock.ClockUpdateListener;

/** Fake implementation of {@link Clock} for testing. */
public class FakeClock extends ObservableNotifier<ClockUpdateListener> implements Clock {
    private static final String TAG = "FakeClock";

    private long currentTime;
    private long elapsedRealtimeMs;

    public FakeClock() {
        this(0, 0);
    }

    public FakeClock(long currentTime, long elapsedRealtime) {
        this.currentTime = currentTime;
        elapsedRealtimeMs = elapsedRealtime;
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
        this.currentTime = currentTime;
        elapsedRealtimeMs = elapsedRealtime;
        onClockUpdated(currentTime, elapsedRealtime);
        return this;
    }

    public void tick() {
        advance(1);
    }

    public void advance(long millis) {
        assertThat(millis).isAtLeast(0L);
        currentTime += millis;
        elapsedRealtimeMs += millis;

        Logger.i(TAG, "Advancing clock to %d", currentTime);
        onClockUpdated(currentTime, elapsedRealtimeMs);
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
        return currentTime;
    }

    @Override
    public long elapsedRealtime() {
        return elapsedRealtimeMs;
    }

    @Override
    public long uptimeMillis() {
        return elapsedRealtimeMs;
    }

    private void onClockUpdated(long currentTime, long elapsedRealtime) {
        notifyListeners(listener -> listener.onClockUpdated(currentTime, elapsedRealtime));
    }

    /** Listener for when the clock time is updated. */
    public interface ClockUpdateListener {
        void onClockUpdated(long newCurrentTime, long newElapsedRealtime);
    }
}
