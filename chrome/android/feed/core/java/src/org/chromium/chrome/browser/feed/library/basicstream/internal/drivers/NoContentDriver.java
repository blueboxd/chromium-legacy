// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.NoContentViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

/** {@link FeatureDriver} for NoContent card. */
public class NoContentDriver extends LeafFeatureDriver {
    private static final String TAG = "NoContentDriver";
    /*@Nullable*/ private NoContentViewHolder mNoContentViewHolder;

    @Override
    public void bind(FeedViewHolder viewHolder) {
        if (isBound()) {
            Logger.wtf(TAG, "Rebinding.");
        }

        checkState(viewHolder instanceof NoContentViewHolder);
        mNoContentViewHolder = (NoContentViewHolder) viewHolder;
        mNoContentViewHolder.bind();
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_NO_CONTENT;
    }

    @Override
    public void unbind() {
        if (mNoContentViewHolder == null) {
            return;
        }

        mNoContentViewHolder.unbind();
        mNoContentViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (mNoContentViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        NoContentViewHolder localViewHolder = mNoContentViewHolder;
        unbind();
        bind(localViewHolder);
    }

    @VisibleForTesting
    boolean isBound() {
        return mNoContentViewHolder != null;
    }

    @Override
    public void onDestroy() {}
}
