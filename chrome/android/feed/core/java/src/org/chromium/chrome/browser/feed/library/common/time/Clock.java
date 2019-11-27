// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.time;

/**
 * Interface of SystemClock; real instances should just delegate the calls to the static methods,
 * while mock instances return values set manually; see {@link android.os.SystemClock}. In addition,
 * this interface also has an instance method for {@link System#currentTimeMillis}, so it can be
 * mocked if needed
 */
/*@DoNotMock("Use org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock instead")*/
public interface Clock {
    /** Number of nanoseconds in a single millisecond. */
    long NS_IN_MS = 1_000_000;

    /**
     * Returns the current system time in milliseconds since January 1, 1970 00:00:00 UTC. This
     * method shouldn't be used for measuring timeouts or other elapsed time measurements, as
     * changing the system time can affect the results.
     *
     * @return the local system time in milliseconds.
     */
    long currentTimeMillis();

    /**
     * Returns milliseconds since boot, including time spent in sleep.
     *
     * @return elapsed milliseconds since boot.
     */
    long elapsedRealtime();

    /**
     * Returns nanoseconds since boot, including time spent in sleep.
     *
     * @return elapsed nanoseconds since boot.
     */
    default long elapsedRealtimeNanos() {
        return NS_IN_MS * elapsedRealtime();
    }

    /**
     * Returns milliseconds since boot, not counting time spent in deep sleep.
     *
     * @return milliseconds of non-sleep uptime since boot.
     */
    long uptimeMillis();
}
