// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.content.Context;
import android.support.v7.widget.RecyclerView.LayoutParams;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for no content card. */
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
