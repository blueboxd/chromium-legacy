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

package com.google.android.libraries.feed.sharedstream.deepestcontenttracker;

import android.support.annotation.VisibleForTesting;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import java.util.ArrayList;
import java.util.List;

/** Tracks the {@code contentId} of the deepest viewed content. */
public class DeepestContentTracker implements ViewDepthProvider {

  private static final String TAG = "DeepestContentTracker";

  private final List</*@Nullable*/ String> contentIds = new ArrayList<>();

  public void updateDeepestContentTracker(int contentPosition, /*@Nullable*/ String contentId) {

    // Fill content ids to size of content position. This is needed in-case we programmatically set
    // scroll position of the recycler view.
    // Add one to contentPosition size in order to more easily perform a set below.
    while (contentIds.size() < contentPosition + 1) {
      contentIds.add(null);
    }

    // Just update the content id of the item in the list.
    contentIds.set(contentPosition, contentId);
  }

  public void removeContentId(int contentPosition) {
    if (contentPosition >= contentIds.size()) {
      return;
    }

    contentIds.remove(contentPosition);
  }

  @VisibleForTesting
  /*@Nullable*/
  String getContentItAtPosition(int position) {
    if (position >= contentIds.size() || position < 0) {
      return null;
    }

    return contentIds.get(position);
  }

  public void reset() {
    contentIds.clear();
  }

  @Override
  /*@Nullable*/
  public String getChildViewDepth() {
    if (contentIds.isEmpty()) {
      return null;
    }

    int i = contentIds.size() - 1;
    while (contentIds.get(i) == null && i > 0) {
      i--;
    }

    return contentIds.get(i);
  }
}
