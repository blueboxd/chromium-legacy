// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import android.content.Context;
import android.support.v7.widget.RecyclerView.LayoutParams;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.common.ui.LayoutUtils;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for no content card. */
public class NoContentViewHolder extends FeedViewHolder {
    private final CardConfiguration cardConfiguration;
    private final View view;

    public NoContentViewHolder(
            CardConfiguration cardConfiguration, Context context, FrameLayout frameLayout) {
        super(frameLayout);
        this.cardConfiguration = cardConfiguration;
        view = LayoutInflater.from(context).inflate(R.layout.no_content, frameLayout);
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
                cardConfiguration.getCardStartMargin(), 0, cardConfiguration.getCardEndMargin(),
                cardConfiguration.getCardBottomMargin());

        view.setBackground(cardConfiguration.getCardBackground());
    }

    @Override
    public void unbind() {}
}
