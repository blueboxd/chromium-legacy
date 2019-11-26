// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

import static com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener.UNKNOWN_SCROLL_DELTA;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener;
import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener.ScrollState;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;

import java.util.HashSet;
import java.util.Set;

/** Class which monitors scrolls and notifies listeners on changes. */
public class ScrollListenerNotifier implements ScrollObserver {
    private static final String TAG = "StreamScrollMonitor";

    private final MainThreadRunner mMainThreadRunner;
    private final Set<ScrollListener> mScrollListeners;
    private final ContentChangedListener mContentChangedListener;

    // Doesn't like adding itself to the scrollobservable
    @SuppressWarnings("initialization")
    public ScrollListenerNotifier(ContentChangedListener childChangeListener,
            ScrollObservable scrollObservable, MainThreadRunner mainThreadRunner) {
        this.mContentChangedListener = childChangeListener;
        this.mMainThreadRunner = mainThreadRunner;

        mScrollListeners = new HashSet<>();

        scrollObservable.addScrollObserver(this);
    }

    public void addScrollListener(ScrollListener listener) {
        mScrollListeners.add(listener);
    }

    public void removeScrollListener(ScrollListener listener) {
        mScrollListeners.remove(listener);
    }

    /**
     * Should be called if a programmatic scroll of the RecyclerView is done. Will notify host with
     * appropriate deltas.
     */
    public void onProgrammaticScroll(RecyclerView recyclerView) {
        mMainThreadRunner.execute(TAG + " onProgrammaticScroll", () -> {
            // Post scroll as this allows users of scroll to retrieve new heights/widths of change.
            onScroll(recyclerView, "", UNKNOWN_SCROLL_DELTA, UNKNOWN_SCROLL_DELTA);
        });
    }

    @ScrollState
    public static int convertRecyclerViewScrollStateToListenerState(int state) {
        switch (state) {
            case RecyclerView.SCROLL_STATE_DRAGGING:
                return ScrollState.DRAGGING;
            case RecyclerView.SCROLL_STATE_SETTLING:
                return ScrollState.SETTLING;
            case RecyclerView.SCROLL_STATE_IDLE:
                return ScrollState.IDLE;
            default:
                Logger.wtf(TAG, "Invalid recycler view scroll state: %d", state);
                return ScrollState.IDLE;
        }
    }

    @Override
    public void onScrollStateChanged(View view, String featureId, int newState, long timestamp) {
        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
            mContentChangedListener.onContentChanged();
        }

        int scrollState = convertRecyclerViewScrollStateToListenerState(newState);
        for (ScrollListener scrollListener : mScrollListeners) {
            scrollListener.onScrollStateChanged(scrollState);
        }
    }

    @Override
    public void onScroll(View view, String featureId, int dx, int dy) {
        for (ScrollListener scrollListener : mScrollListeners) {
            scrollListener.onScrolled(dx, dy);
        }
    }
}
