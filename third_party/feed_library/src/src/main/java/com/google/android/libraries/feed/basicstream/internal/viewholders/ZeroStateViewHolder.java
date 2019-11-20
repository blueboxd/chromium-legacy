// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.common.ui.LayoutUtils;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for zero state. */
public class ZeroStateViewHolder extends FeedViewHolder {

  private final View zeroStateView;
  private final View loadingSpinner;
  private final View actionButton;
  private final CardConfiguration cardConfiguration;

  public ZeroStateViewHolder(
      Context context, FrameLayout frameLayout, CardConfiguration cardConfiguration) {
    super(frameLayout);
    View view = LayoutInflater.from(context).inflate(R.layout.zero_state, frameLayout);

    loadingSpinner = view.findViewById(R.id.loading_spinner);
    zeroStateView = view.findViewById(R.id.zero_state);
    actionButton = view.findViewById(R.id.action_button);
    this.cardConfiguration = cardConfiguration;
  }

  public void bind(OnClickListener onClickListener, boolean showSpinner) {
    View noContentView = itemView.findViewById(R.id.no_content_container);
    setCardLayout(noContentView, cardConfiguration);

    actionButton.setOnClickListener(onClickListener);
    showSpinner(showSpinner);
  }

  @Override
  public void unbind() {
    // Clear OnClickListener to null to allow for GC.
    actionButton.setOnClickListener(null);

    // Set clickable to false as setting OnClickListener to null sets clickable to true.
    actionButton.setClickable(false);
  }

  public void showSpinner(boolean showSpinner) {
    loadingSpinner.setVisibility(showSpinner ? View.VISIBLE : View.GONE);
    zeroStateView.setVisibility(showSpinner ? View.GONE : View.VISIBLE);
  }

  private void setCardLayout(View cardView, CardConfiguration cardConfiguration) {
    // Need to reset padding here.  Setting a background can affect padding so if we switch from
    // a background which has padding to one that does not, then the padding needs to be removed.
    cardView.setPadding(0, 0, 0, 0);

    cardView.setBackground(cardConfiguration.getCardBackground());

    ViewGroup.LayoutParams layoutParams = cardView.getLayoutParams();
    if (layoutParams == null) {
      layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    } else if (!(layoutParams instanceof MarginLayoutParams)) {
      layoutParams = new LayoutParams(layoutParams);
    }
    cardView.setLayoutParams(layoutParams);

    LayoutUtils.setMarginsRelative(
        (MarginLayoutParams) layoutParams,
        cardConfiguration.getCardStartMargin(),
        0, // The top margin is not configurable by the host, so is always zero.
        cardConfiguration.getCardEndMargin(),
        cardConfiguration.getCardBottomMargin());
  }
}
