// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.scroll;

import android.view.View;

import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollTracker;

/** A @link{ScrollTracker} used by BasicStream */
public class BasicStreamScrollTracker extends ScrollTracker {
    private final ScrollLogger mScrollLogger;
    private final ScrollObserver mScrollObserver;
    private final ScrollObservable mScrollObservable;

    public BasicStreamScrollTracker(MainThreadRunner mainThreadRunner, ScrollLogger scrollLogger,
            Clock clock, ScrollObservable scrollObservable) {
        super(mainThreadRunner, clock);
        this.mScrollLogger = scrollLogger;
        this.mScrollObservable = scrollObservable;
        this.mScrollObserver = new BasicStreamScrollObserver();
        mScrollObservable.addScrollObserver(mScrollObserver);
    }

    @Override
    protected void onScrollEvent(int scrollAmount, long timestamp) {
        mScrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }

    @Override
    public void onUnbind() {
        super.onUnbind();
        mScrollObservable.removeScrollObserver(mScrollObserver);
    }

    private class BasicStreamScrollObserver implements ScrollObserver {
        @Override
        public void onScrollStateChanged(
                View view, String featureId, int newState, long timestamp) {}

        @Override
        public void onScroll(View view, String featureId, int dx, int dy) {
            trackScroll(dx, dy);
        }
    }
}
