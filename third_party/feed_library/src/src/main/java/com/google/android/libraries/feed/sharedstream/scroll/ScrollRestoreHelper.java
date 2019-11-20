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

package com.google.android.libraries.feed.sharedstream.scroll;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;

/** Helper for restoring scroll positions. */
public class ScrollRestoreHelper {
  private static final String TAG = "ScrollUtils";
  private static final boolean ABANDON_RESTORE_BELOW_FOLD_DEFAULT = true;
  private static final long ABANDON_RESTORE_BELOW_FOLD_THRESHOLD_DEFAULT = 10L;

  /** Private constructor to prevent instantiation. */
  private ScrollRestoreHelper() {}

  /**
   * Returns a {@link ScrollState} for scroll position restore, or {@literal null} if the scroll
   * position can't or shouldn't be restored.
   *
   * @param currentHeaderCount The amount of headers which appear before Stream content.
   */
  /*@Nullable*/
  public static ScrollState getScrollStateForScrollRestore(
      LinearLayoutManager layoutManager, Configuration configuration, int currentHeaderCount) {
    int firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition();
    View firstVisibleView = layoutManager.findViewByPosition(firstVisibleItemPosition);
    int firstVisibleTop =
        firstVisibleView == null ? RecyclerView.NO_POSITION : firstVisibleView.getTop();
    int lastVisibleItemPosition = layoutManager.findLastVisibleItemPosition();

    return getScrollStateForScrollRestore(
        firstVisibleItemPosition,
        firstVisibleTop,
        lastVisibleItemPosition,
        configuration,
        currentHeaderCount);
  }

  /*@Nullable*/
  public static ScrollState getScrollStateForScrollRestore(
      int firstVisibleItemPosition,
      int firstVisibleTop,
      int lastVisibleItemPosition,
      Configuration configuration,
      int currentHeaderCount) {

    // If either of the firstVisibleItemPosition or firstVisibleTop are unknown, we shouldn't
    // restore scroll, so return null.
    if (firstVisibleItemPosition == RecyclerView.NO_POSITION
        || firstVisibleTop == RecyclerView.NO_POSITION) {
      return null;
    }

    // Determine if we can restore past the fold.
    if (configuration.getValueOrDefault(
        ConfigKey.ABANDON_RESTORE_BELOW_FOLD, ABANDON_RESTORE_BELOW_FOLD_DEFAULT)) {
      long threshold =
          configuration.getValueOrDefault(
              ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD,
              ABANDON_RESTORE_BELOW_FOLD_THRESHOLD_DEFAULT);

      if (lastVisibleItemPosition == RecyclerView.NO_POSITION
          || lastVisibleItemPosition - currentHeaderCount > threshold) {
        Logger.w(
            TAG,
            "Abandoning scroll due to fold threshold.  Bottom scroll index: %d, Header "
                + "count: %d, Configured Threshold: %d",
            lastVisibleItemPosition,
            currentHeaderCount,
            threshold);
        return null;
      }
    }

    return ScrollState.newBuilder()
        .setPosition(firstVisibleItemPosition)
        .setOffset(firstVisibleTop)
        .build();
  }
}
