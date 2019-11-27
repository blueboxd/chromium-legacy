// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.testing;

import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.LeafFeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;

/** Fake for {@link LeafFeatureDriver}. */
public class FakeLeafFeatureDriver extends LeafFeatureDriver {
    private final int mItemViewType;

    private FakeLeafFeatureDriver(int itemViewType) {
        this.mItemViewType = itemViewType;
    }

    @Override
    public void bind(FeedViewHolder viewHolder) {}

    @Override
    public void unbind() {}

    @Override
    public void maybeRebind() {}

    @Override
    @ViewHolderType
    public int getItemViewType() {
        return mItemViewType;
    }

    @Override
    public long itemId() {
        return hashCode();
    }

    @Override
    public void onDestroy() {}

    @Override
    public LeafFeatureDriver getLeafFeatureDriver() {
        return this;
    }

    public static class Builder {
        @ViewHolderType
        private int mItemViewType = ViewHolderType.TYPE_CARD;

        public Builder setItemViewType(@ViewHolderType int viewType) {
            mItemViewType = viewType;
            return this;
        }

        public LeafFeatureDriver build() {
            return new FakeLeafFeatureDriver(mItemViewType);
        }
    }
}
