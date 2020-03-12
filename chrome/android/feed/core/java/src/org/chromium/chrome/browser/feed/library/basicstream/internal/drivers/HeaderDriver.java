// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.HeaderViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.SwipeNotifier;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

/** {@link FeatureDriver} for headers. */
public class HeaderDriver extends LeafFeatureDriver {
    private static final String TAG = "HeaderDriver";

    private final Header mHeader;
    private final SwipeNotifier mSwipeNotifier;
    @Nullable
    private HeaderViewHolder mHeaderViewHolder;

    public HeaderDriver(Header header, SwipeNotifier swipeNotifier) {
        this.mHeader = header;
        this.mSwipeNotifier = swipeNotifier;
    }

    @Override
    public void bind(FeedViewHolder viewHolder) {
        if (isBound()) {
            if (viewHolder == this.mHeaderViewHolder) {
                Logger.e(TAG, "Being rebound to the previously bound viewholder");
                return;
            }
            unbind();
        }

        checkState(viewHolder instanceof HeaderViewHolder);
        mHeaderViewHolder = (HeaderViewHolder) viewHolder;
        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);
    }

    @Override
    public void unbind() {
        if (mHeaderViewHolder == null) {
            return;
        }

        mHeaderViewHolder.unbind();
        mHeaderViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (mHeaderViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        HeaderViewHolder localViewHolder = mHeaderViewHolder;
        unbind();
        bind(localViewHolder);
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_HEADER;
    }

    @Override
    public void onDestroy() {}

    public Header getHeader() {
        return mHeader;
    }

    @VisibleForTesting
    boolean isBound() {
        return mHeaderViewHolder != null;
    }
}
