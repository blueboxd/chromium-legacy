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

package com.google.android.libraries.feed.basicstream.internal;

import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;

/**
 * {@link DefaultItemAnimator} implementation that notifies the given {@link ContentChangedListener}
 * after animations occur.
 */
public class StreamItemAnimator extends DefaultItemAnimator {

  private final ContentChangedListener contentChangedListener;
  private boolean isStreamContentVisible;

  public StreamItemAnimator(ContentChangedListener contentChangedListener) {
    this.contentChangedListener = contentChangedListener;
  }

  @Override
  public void onAnimationFinished(RecyclerView.ViewHolder viewHolder) {
    super.onAnimationFinished(viewHolder);
    contentChangedListener.onContentChanged();
  }

  public void setStreamVisibility(boolean isStreamContentVisible) {
    if (this.isStreamContentVisible == isStreamContentVisible) {
      return;
    }

    if (isStreamContentVisible) {
      // Ending animations so that if any content is animating out the RecyclerView will be able to
      // remove those views. This can occur if a user quickly presses hide and then show on the
      // stream.
      endAnimations();
    }

    this.isStreamContentVisible = isStreamContentVisible;
  }

  @VisibleForTesting
  public boolean getStreamContentVisibility() {
    return isStreamContentVisible;
  }
}
