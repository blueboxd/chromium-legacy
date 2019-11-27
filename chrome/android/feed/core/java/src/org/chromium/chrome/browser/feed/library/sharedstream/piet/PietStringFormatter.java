// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import android.text.format.DateUtils;

import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.piet.host.StringFormatter;

import javax.inject.Inject;

/** Formats strings for Piet. */
public class PietStringFormatter implements StringFormatter {
    private final Clock mClock;

    @Inject
    public PietStringFormatter(Clock clock) {
        this.mClock = clock;
    }

    @Override
    public String getRelativeElapsedString(long elapsedTimeMillis) {
        return DateUtils
                .getRelativeTimeSpanString(mClock.currentTimeMillis() - elapsedTimeMillis,
                        mClock.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS)
                .toString();
    }
}
