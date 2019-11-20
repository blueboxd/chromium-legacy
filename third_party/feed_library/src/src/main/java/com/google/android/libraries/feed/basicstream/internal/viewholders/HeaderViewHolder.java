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


import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.common.logging.Logger;

/** {@link FeedViewHolder} for headers. */
public class HeaderViewHolder extends FeedViewHolder implements SwipeableViewHolder {

  private static final String TAG = "HeaderViewHolder";
  private final FrameLayout frameLayout;

  /*@Nullable*/ private Header header;
  /*@Nullable*/ private SwipeNotifier swipeNotifier;

  public HeaderViewHolder(FrameLayout itemView) {
    super(itemView);
    this.frameLayout = itemView;
  }

  public void bind(Header header, SwipeNotifier swipeNotifier) {
    this.header = header;
    this.swipeNotifier = swipeNotifier;
    ViewParent parent = header.getView().getParent();
    if (parent == frameLayout) {
      return;
    }
    // If header was bound to another HeaderViewHolder but not unbound properly, remove it from its
    // parent view.
    if (parent != null) {
      ((ViewGroup) parent).removeView(header.getView());
    }
    frameLayout.addView(header.getView());
  }

  @Override
  public void unbind() {
    frameLayout.removeAllViews();
    this.header = null;
    this.swipeNotifier = null;
  }

  @Override
  public boolean canSwipe() {
    if (header == null) {
      Logger.w(TAG, "canSwipe should not be called before viewholder is bound.");

      // Instead of crashing if viewholder is not bound, disable swiping behavior.
      return false;
    }
    return header.isDismissible();
  }

  @Override
  public void onSwiped() {
    if (swipeNotifier == null) {
      Logger.w(TAG, "onSwiped should not be called before viewholder is bound.");

      // Instead of crashing if viewholder is not bound, disable swiping behavior.
      return;
    }

    swipeNotifier.onSwiped();
  }
}
