// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView.LayoutParams;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.feed.R;

/** {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} for no content card. */
public class NoContentViewHolder extends FeedViewHolder {
    private final CardConfiguration mCardConfiguration;
    private final View mView;

    public NoContentViewHolder(
            CardConfiguration cardConfiguration, Context context, FrameLayout frameLayout) {
        super(frameLayout);
        this.mCardConfiguration = cardConfiguration;
        mView = LayoutInflater.from(context).inflate(R.layout.no_content, frameLayout);
    }

    public void bind() {
        ViewGroup.LayoutParams layoutParams = itemView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            itemView.setLayoutParams(layoutParams);
        } else if (!(layoutParams instanceof MarginLayoutParams)) {
            layoutParams = new LayoutParams(layoutParams);
            itemView.setLayoutParams(layoutParams);
        }
        LayoutUtils.setMarginsRelative((MarginLayoutParams) layoutParams,
                mCardConfiguration.getCardStartMargin(), 0, mCardConfiguration.getCardEndMargin(),
                mCardConfiguration.getCardBottomMargin());

        mView.setBackground(mCardConfiguration.getCardBackground());
    }

    @Override
    public void unbind() {}
}
