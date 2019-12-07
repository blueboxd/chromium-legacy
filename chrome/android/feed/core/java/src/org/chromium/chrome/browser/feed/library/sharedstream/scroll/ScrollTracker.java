// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.concurrent.CancelableTask;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.Clock;

/**
 * Helper which is able to track a Scroll and aggregate them before sending the scroll events
 * events.
 */
public abstract class ScrollTracker {
    private static final String TAG = "ScrollTracker";
    // onScroll events are very noisy, so we collate them together to avoid over-reporting scrolls.
    private static final long SCROLL_EVENT_COLLATE_MILLIS = 200L;

    private final MainThreadRunner mMainThreadRunner;
    private final Clock mClock;

    @Nullable
    protected ScrollNotifier mScrollNotifier;
    @Nullable
    protected CancelableTask mTask;

    public ScrollTracker(MainThreadRunner mainThreadRunner, Clock clock) {
        this.mMainThreadRunner = mainThreadRunner;
        this.mClock = clock;
    }

    public void onUnbind() {
        ScrollTracker.ScrollNotifier localScrollNotifier = mScrollNotifier;
        if (localScrollNotifier != null) {
            if (mTask != null) {
                mTask.cancel();
            }
            localScrollNotifier.run();
        }
    }

    protected void trackScroll(int dx, int dy) {
        boolean positiveScroll;
        int amount;
        if (dy == 0) {
            return;
        }
        positiveScroll = dy > 0;
        amount = dy;

        int previousTotalScroll = 0;
        ScrollNotifier previousScrollNotifier = mScrollNotifier;
        if (previousScrollNotifier != null && !(mTask != null && mTask.canceled())) {
            if (previousScrollNotifier.mPositiveScroll == positiveScroll) {
                // Same direction, so merge the existing scroll with the new one.
                previousTotalScroll = previousScrollNotifier.mScrollAmount;
                if (mTask != null) {
                    mTask.cancel();
                }
            }
        }

        amount += previousTotalScroll;
        mScrollNotifier = new ScrollNotifier(positiveScroll, amount, mClock.currentTimeMillis());
        mTask = mMainThreadRunner.executeWithDelay(
                TAG, mScrollNotifier, SCROLL_EVENT_COLLATE_MILLIS);
    }

    protected abstract void onScrollEvent(int scrollAmount, long timestamp);

    private class ScrollNotifier implements Runnable {
        final boolean mPositiveScroll;
        final int mScrollAmount;
        final long mTimestamp;

        public ScrollNotifier(boolean positiveScroll, int scrollAmount, long timestamp) {
            this.mPositiveScroll = positiveScroll;
            this.mScrollAmount = scrollAmount;
            this.mTimestamp = timestamp;
        }

        @Override
        public void run() {
            onScrollEvent(mScrollAmount, mTimestamp);
            if (mScrollNotifier == this) {
                mScrollNotifier = null;
                if (mTask != null) {
                    mTask.cancel();
                }
            }
        }
    }
}
