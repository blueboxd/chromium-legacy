// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.scroll;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollListenerNotifier;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollRestoreHelper;

/**
 * Class which is able to save a scroll position for a RecyclerView and restore to that scroll
 * position at a later time.
 */
public class ScrollRestorer {
    private static final String TAG = "ScrollRestorer";

    private final Configuration mConfiguration;
    private final RecyclerView mRecyclerView;
    private final ScrollListenerNotifier mScrollListenerNotifier;
    private final int mScrollPosition;
    private final int mScrollOffset;

    private boolean mCanRestore;

    public ScrollRestorer(Configuration configuration, RecyclerView recyclerView,
            ScrollListenerNotifier scrollListenerNotifier,
            /*@Nullable*/ ScrollState scrollState) {
        this.mConfiguration = configuration;
        this.mRecyclerView = recyclerView;
        this.mScrollListenerNotifier = scrollListenerNotifier;

        if (scrollState != null) {
            mCanRestore = true;
            mScrollPosition = scrollState.getPosition();
            mScrollOffset = scrollState.getOffset();
        } else {
            this.mScrollPosition = 0;
            this.mScrollOffset = 0;
        }
    }

    private ScrollRestorer(Configuration configuration, RecyclerView recyclerView,
            ScrollListenerNotifier scrollListenerNotifier) {
        mCanRestore = false;
        this.mConfiguration = configuration;
        this.mRecyclerView = recyclerView;
        this.mScrollListenerNotifier = scrollListenerNotifier;
        mScrollPosition = 0;
        mScrollOffset = 0;
    }

    /**
     * Creates a {@code StreamRestorer} which will never restore state. This can be used to fulfill
     * {@code StreamRestorer} requests when no restore state is present.
     */
    public static ScrollRestorer nonRestoringRestorer(Configuration configuration,
            RecyclerView recyclerView, ScrollListenerNotifier scrollListenerNotifier) {
        return new ScrollRestorer(configuration, recyclerView, scrollListenerNotifier);
    }

    /**
     * Disables the ability of the {@code ScrollRestorer} from restoring the scroll. This should be
     * call if the previous scroll position is no longer valid. An example use case of this would be
     * if the restoring session is no longer valid.
     */
    public void abandonRestoringScroll() {
        mCanRestore = false;
    }

    /**
     * Attempts to restore scroll position if possible. If the scroll position has already been
     * restored, then this method will no-op.
     */
    public void maybeRestoreScroll() {
        if (!mCanRestore) {
            return;
        }
        Logger.d(TAG, "Restoring scroll");
        getLayoutManager().scrollToPositionWithOffset(mScrollPosition, mScrollOffset);
        mScrollListenerNotifier.onProgrammaticScroll(mRecyclerView);
        mCanRestore = false;
    }

    /**
     * Returns a bundle which can be used for restoring scroll state on an activity restart.
     *
     * @param currentHeaderCount The amount of headers which appear before Stream content.
     */
    /*@Nullable*/
    public ScrollState getScrollStateForScrollRestore(int currentHeaderCount) {
        return ScrollRestoreHelper.getScrollStateForScrollRestore(
                getLayoutManager(), mConfiguration, currentHeaderCount);
    }

    private LinearLayoutManager getLayoutManager() {
        checkState(mRecyclerView.getLayoutManager() instanceof LinearLayoutManager,
                "Scroll state can only be restored when using a LinearLayoutManager.");
        return checkNotNull((LinearLayoutManager) mRecyclerView.getLayoutManager());
    }
}
