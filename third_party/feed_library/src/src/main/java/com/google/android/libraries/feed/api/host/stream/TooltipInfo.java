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

package com.google.android.libraries.feed.api.host.stream;

import android.support.annotation.StringDef;

/** All the information necessary to render a tooltip. */
public interface TooltipInfo {

  /** The list of features that the tooltip can highlight. */
  @StringDef({FeatureName.UNKNOWN, FeatureName.CARD_MENU_TOOLTIP})
  @interface FeatureName {
    String UNKNOWN = "unknown";
    String CARD_MENU_TOOLTIP = "card_menu_tooltip";
  }

  /** Returns the text to display in the tooltip. */
  String getLabel();

  /** Returns the talkback string to attach to tooltip. */
  String getAccessibilityLabel();

  /** Returns the Feature that the tooltip should reference. */
  @FeatureName
  String getFeatureName();

  /**
   * Returns the number of dp units that the tooltip arrow should point to below the center of the
   * top of the view it is referencing.
   */
  int getTopInset();

  /**
   * Returns the number of dp units that the tooltip arrow should point to above the center of the
   * bottom of the view it is referencing.
   */
  int getBottomInset();
}
