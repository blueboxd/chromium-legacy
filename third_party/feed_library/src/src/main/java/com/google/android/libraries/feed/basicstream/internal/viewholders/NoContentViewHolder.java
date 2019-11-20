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
    LayoutUtils.setMarginsRelative(
        (MarginLayoutParams) layoutParams,
        cardConfiguration.getCardStartMargin(),
        0,
        cardConfiguration.getCardEndMargin(),
        cardConfiguration.getCardBottomMargin());

    view.setBackground(cardConfiguration.getCardBackground());
  }

  @Override
  public void unbind() {}
}
