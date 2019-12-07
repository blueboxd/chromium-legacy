// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

/** {@link FeedViewHolder} for headers. */
public class HeaderViewHolder extends FeedViewHolder implements SwipeableViewHolder {
    private static final String TAG = "HeaderViewHolder";
    private final FrameLayout mFrameLayout;

    @Nullable
    private Header mHeader;
    @Nullable
    private SwipeNotifier mSwipeNotifier;

    public HeaderViewHolder(FrameLayout itemView) {
        super(itemView);
        this.mFrameLayout = itemView;
    }

    public void bind(Header header, SwipeNotifier swipeNotifier) {
        this.mHeader = header;
        this.mSwipeNotifier = swipeNotifier;
        ViewParent parent = header.getView().getParent();
        if (parent == mFrameLayout) {
            return;
        }
        // If header was bound to another HeaderViewHolder but not unbound properly, remove it from
        // its parent view.
        if (parent != null) {
            ((ViewGroup) parent).removeView(header.getView());
        }
        mFrameLayout.addView(header.getView());
    }

    @Override
    public void unbind() {
        mFrameLayout.removeAllViews();
        this.mHeader = null;
        this.mSwipeNotifier = null;
    }

    @Override
    public boolean canSwipe() {
        if (mHeader == null) {
            Logger.w(TAG, "canSwipe should not be called before viewholder is bound.");

            // Instead of crashing if viewholder is not bound, disable swiping behavior.
            return false;
        }
        return mHeader.isDismissible();
    }

    @Override
    public void onSwiped() {
        if (mSwipeNotifier == null) {
            Logger.w(TAG, "onSwiped should not be called before viewholder is bound.");

            // Instead of crashing if viewholder is not bound, disable swiping behavior.
            return;
        }

        mSwipeNotifier.onSwiped();
    }
}
