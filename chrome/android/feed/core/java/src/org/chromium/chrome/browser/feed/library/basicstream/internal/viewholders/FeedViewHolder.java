// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for the Feed. */
public abstract class FeedViewHolder extends ViewHolder {
    public FeedViewHolder(View view) {
        super(view);
    }

    public abstract void unbind();
}
