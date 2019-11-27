// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for zero state. */
public class ZeroStateViewHolder extends FeedViewHolder {
    private final View mZeroStateView;
    private final View mLoadingSpinner;
    private final View mActionButton;
    private final CardConfiguration mCardConfiguration;

    public ZeroStateViewHolder(
            Context context, FrameLayout frameLayout, CardConfiguration cardConfiguration) {
        super(frameLayout);
        View view = LayoutInflater.from(context).inflate(R.layout.zero_state, frameLayout);

        mLoadingSpinner = view.findViewById(R.id.loading_spinner);
        mZeroStateView = view.findViewById(R.id.zero_state);
        mActionButton = view.findViewById(R.id.action_button);
        this.mCardConfiguration = cardConfiguration;
    }

    public void bind(OnClickListener onClickListener, boolean showSpinner) {
        View noContentView = itemView.findViewById(R.id.no_content_container);
        setCardLayout(noContentView, mCardConfiguration);

        mActionButton.setOnClickListener(onClickListener);
        showSpinner(showSpinner);
    }

    @Override
    public void unbind() {
        // Clear OnClickListener to null to allow for GC.
        mActionButton.setOnClickListener(null);

        // Set clickable to false as setting OnClickListener to null sets clickable to true.
        mActionButton.setClickable(false);
    }

    public void showSpinner(boolean showSpinner) {
        mLoadingSpinner.setVisibility(showSpinner ? View.VISIBLE : View.GONE);
        mZeroStateView.setVisibility(showSpinner ? View.GONE : View.VISIBLE);
    }

    private void setCardLayout(View cardView, CardConfiguration cardConfiguration) {
        // Need to reset padding here.  Setting a background can affect padding so if we switch from
        // a background which has padding to one that does not, then the padding needs to be
        // removed.
        cardView.setPadding(0, 0, 0, 0);

        cardView.setBackground(cardConfiguration.getCardBackground());

        ViewGroup.LayoutParams layoutParams = cardView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        } else if (!(layoutParams instanceof MarginLayoutParams)) {
            layoutParams = new LayoutParams(layoutParams);
        }
        cardView.setLayoutParams(layoutParams);

        LayoutUtils.setMarginsRelative((MarginLayoutParams) layoutParams,
                cardConfiguration.getCardStartMargin(),
                0, // The top margin is not configurable by the host, so is always zero.
                cardConfiguration.getCardEndMargin(), cardConfiguration.getCardBottomMargin());
    }
}
