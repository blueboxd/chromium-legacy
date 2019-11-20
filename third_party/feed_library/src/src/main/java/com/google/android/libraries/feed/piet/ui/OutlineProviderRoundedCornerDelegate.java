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

package com.google.android.libraries.feed.piet.ui;

import android.view.ViewGroup;

/**
 * Rounding delegate for the outline provider strategy.
 *
 * <p>Adds logic to clip rounded corner views to their outline. {@link RoundedCornerWrapperView}
 * decides which rounding strategy to use and sets the appropriate delegate.
 */
class OutlineProviderRoundedCornerDelegate extends RoundedCornerDelegate {

  /**
   * Sets clipToOutline to true.
   *
   * <p>Setting it in an initializer rather than a constructor means a new instance of this delegate
   * doesn't need to be created every time an outline provider is needed.
   */
  @Override
  public void initializeForView(ViewGroup view) {
    view.setClipToOutline(true);
    view.setClipChildren(true);
  }

  /** Reset clipToOutline to false (the default) when this strategy stops being used. */
  @Override
  public void destroy(ViewGroup view) {
    view.setClipToOutline(false);
    view.setClipChildren(false);
  }
}
