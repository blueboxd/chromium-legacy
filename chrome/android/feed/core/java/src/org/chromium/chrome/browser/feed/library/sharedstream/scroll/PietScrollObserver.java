// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import org.chromium.chrome.browser.feed.library.piet.FrameAdapter;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver;

/** Scroll observer that triggers Piet scroll actions. */
public class PietScrollObserver implements ScrollObserver {
    private final FrameAdapter mFrameAdapter;
    private final View mViewport;
    private final ScrollObservable mScrollObservable;
    private final FirstDrawTrigger mFirstDrawTrigger;

    /**
     * Construct a PietScrollObserver.
     *
     * @param frameAdapter Piet FrameAdapter which will have view actions triggered by this scroll
     *     observer
     * @param viewport the view containing the frameAdapter, the bounds of which determine whether
     *         the
     *     frame is visible
     * @param scrollObservable used to get the scroll state to ensure scrolling is idle before
     *     triggering
     */
    @SuppressWarnings("initialization") // Doesn't like the OnAttachStateChangeListener.
    public PietScrollObserver(
            FrameAdapter frameAdapter, View viewport, ScrollObservable scrollObservable) {
        this.mFrameAdapter = frameAdapter;
        this.mViewport = viewport;
        this.mScrollObservable = scrollObservable;

        mFirstDrawTrigger = new FirstDrawTrigger();

        frameAdapter.getFrameContainer().addOnAttachStateChangeListener(
                new OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        installFirstDrawTrigger();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {
                        uninstallFirstDrawTrigger();
                    }
                });
    }

    @Override
    public void onScrollStateChanged(View view, String featureId, int newState, long timestamp) {
        // There was logic in here previously ensuring that the state had changed; however, you can
        // have a case where the feed is idle and you fling the feed, and it goes to idle again
        // without triggering the observer during scrolling. Just triggering this every time the
        // feed comes to rest appears to have the desired behavior. We can think about also
        // triggering while the feed is scrolling; not sure how frequently the observer triggers
        // during scroll.
        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
            mFrameAdapter.triggerViewActions(mViewport);
        }
    }

    @Override
    public void onScroll(View view, String featureId, int dx, int dy) {
        // no-op
    }

    /** Install the first-draw triggering observer. */
    public void installFirstDrawTrigger() {
        mFrameAdapter.getFrameContainer().getViewTreeObserver().addOnGlobalLayoutListener(
                mFirstDrawTrigger);
    }

    /** Uninstall the first-draw triggering observer. */
    public void uninstallFirstDrawTrigger() {
        mFrameAdapter.getFrameContainer().getViewTreeObserver().removeOnGlobalLayoutListener(
                mFirstDrawTrigger);
    }

    class FirstDrawTrigger implements OnGlobalLayoutListener {
        @Override
        public void onGlobalLayout() {
            uninstallFirstDrawTrigger();

            if (mScrollObservable.getCurrentScrollState() == RecyclerView.SCROLL_STATE_IDLE) {
                mFrameAdapter.triggerViewActions(mViewport);
            }
        }
    }
}
