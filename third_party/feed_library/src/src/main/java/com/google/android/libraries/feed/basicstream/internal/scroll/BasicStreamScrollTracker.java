// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.scroll;

import android.view.View;

import com.google.android.libraries.feed.api.host.logging.ScrollType;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollLogger;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollTracker;

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
