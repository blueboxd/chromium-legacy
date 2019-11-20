// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.sharedstream.publicapi.scroll;

import android.view.View;

/** Observer which gets notified about scroll events. */
public interface ScrollObserver {

  /**
   * Callback with the new state of the scrollable container.
   *
   * @param view the view being scrolled.
   * @param featureId the id of the view where the scroll originated from; optional.
   * @param newState one of the RecyclerView SCROLLING_STATE_fields.
   * @param timestamp the time in ms when the scroll state change occurred.
   */
  default void onScrollStateChanged(View view, String featureId, int newState, long timestamp) {}

  /**
   * Callback with the change in x/y from the most recent scroll events.
   *
   * @param view the view being scrolled.
   * @param featureId the id of the view where the scroll originated from; optional.
   * @param dx the amount scrolled in the x direction.
   * @param dy the amount scrolled in the y direction.
   */
  void onScroll(View view, String featureId, int dx, int dy);
}
