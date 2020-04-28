// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.scroll;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Monitors and dispatches scroll events to the registered listeners.
 *
 * <p>Note: This is intentionally unscoped, because we want features to be able to manage their
 * scroll locally, instead of having a singleton ScrollMonitor.
 */
public class BasicStreamScrollMonitor
        extends RecyclerView.OnScrollListener implements ScrollObservable {
    private final Set<ScrollObserver> mScrollObservers;
    private final Clock mClock;

    private int mCurrentScrollState = RecyclerView.SCROLL_STATE_IDLE;

    public BasicStreamScrollMonitor(Clock clock) {
        this.mClock = clock;
        mScrollObservers = Collections.synchronizedSet(new HashSet<>());
    }

    @Override
    public void addScrollObserver(ScrollObserver scrollObserver) {
        mScrollObservers.add(scrollObserver);
    }

    @Override
    public void removeScrollObserver(ScrollObserver scrollObserver) {
        mScrollObservers.remove(scrollObserver);
    }

    @Override
    public int getCurrentScrollState() {
        return mCurrentScrollState;
    }

    public int getObserverCount() {
        return mScrollObservers.size();
    }

    /**
     * Notify the monitor of a scroll state change event that should be dispatched to the observers.
     */
    @Override
    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
        mCurrentScrollState = newState;
        for (ScrollObserver observer : mScrollObservers) {
            observer.onScrollStateChanged(recyclerView, "", newState, mClock.currentTimeMillis());
        }
    }

    /** Notify the monitor of a scroll event that should be dispatched to its observers. */
    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        for (ScrollObserver observer : mScrollObservers) {
            observer.onScroll(recyclerView, "", dx, dy);
        }
    }
}
