// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.time;

import android.os.Build;
import android.os.SystemClock;

/**
 * Implementation of {@link Clock} that delegates to the system clock.
 *
 * <p>This class is intended for use only in contexts where injection is impossible. Where possible,
 * prefer to simply inject a {@link Clock}.
 */
public class SystemClockImpl implements Clock {
    @Override
    public long currentTimeMillis() {
        return System.currentTimeMillis();
    }

    @Override
    public long elapsedRealtime() {
        return SystemClock.elapsedRealtime();
    }

    @Override
    public long elapsedRealtimeNanos() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            try {
                return SystemClock.elapsedRealtimeNanos();
            } catch (NoSuchMethodError ignoredError) {
                // Some vendors have a SystemClock that doesn't contain the method even though the
                // SDK should contain it. Fall through to the alternate version.
            }
        }
        return SystemClock.elapsedRealtime() * NS_IN_MS;
    }

    @Override
    public long uptimeMillis() {
        return SystemClock.uptimeMillis();
    }
}
