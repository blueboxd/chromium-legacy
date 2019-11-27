// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;

/** A {@link FeatureDriver} that can bind to a {@link FeedViewHolder}. */
public abstract class LeafFeatureDriver implements FeatureDriver {
    @Override
    public LeafFeatureDriver getLeafFeatureDriver() {
        return this;
    }

    /** Bind to the given {@link FeedViewHolder}. */
    public abstract void bind(FeedViewHolder viewHolder);

    /**
     * Returns a {@link ViewHolderType} that corresponds to the type of {@link FeedViewHolder} that
     * can be bound to.
     */
    @ViewHolderType
    public abstract int getItemViewType();

    /** Returns an ID corresponding to this item, typically a hashcode. */
    public long itemId() {
        return hashCode();
    }

    /**
     * Called when the {@link
     * org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder} is
     * offscreen/unbound.
     *
     * <p>Note: {@link LeafFeatureDriver} instances should do work they need to to unbind themselves
     * and their previously bound {@link FeedViewHolder}.
     */
    public abstract void unbind();

    /**
     * This will cause the FeatureHolder associated with the Driver to rebind (unbind followed by
     * bind).
     */
    public abstract void maybeRebind();

    /** Returns the contentId of the {@link LeafFeatureDriver} if it has one. */
    /*@Nullable*/
    public String getContentId() {
        return null;
    }
}
