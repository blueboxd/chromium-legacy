// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.Validators.checkState;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.NoContentViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType;
import com.google.android.libraries.feed.common.logging.Logger;

/** {@link FeatureDriver} for NoContent card. */
public class NoContentDriver extends LeafFeatureDriver {
    private static final String TAG = "NoContentDriver";
    /*@Nullable*/ private NoContentViewHolder noContentViewHolder;

    @Override
    public void bind(FeedViewHolder viewHolder) {
        if (isBound()) {
            Logger.wtf(TAG, "Rebinding.");
        }

        checkState(viewHolder instanceof NoContentViewHolder);
        noContentViewHolder = (NoContentViewHolder) viewHolder;
        noContentViewHolder.bind();
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_NO_CONTENT;
    }

    @Override
    public void unbind() {
        if (noContentViewHolder == null) {
            return;
        }

        noContentViewHolder.unbind();
        noContentViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (noContentViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        NoContentViewHolder localViewHolder = noContentViewHolder;
        unbind();
        bind(localViewHolder);
    }

    @VisibleForTesting
    boolean isBound() {
        return noContentViewHolder != null;
    }

    @Override
    public void onDestroy() {}
}
