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

import android.support.annotation.IntDef;

/** Interface for callbacks for tooltip events. */
public interface TooltipCallbackApi {

  /** The ways a tooltip can be dismissed. */
  @IntDef({
    TooltipDismissType.UNKNOWN,
    TooltipDismissType.TIMEOUT,
    TooltipDismissType.CLICK,
    TooltipDismissType.CLICK_OUTSIDE,
  })
  @interface TooltipDismissType {
    int UNKNOWN = 0;
    int TIMEOUT = 1;
    int CLICK = 2;
    int CLICK_OUTSIDE = 3;
  }

  /** The callback that will be triggered when the tooltip is shown. */
  void onShow();

  /**
   * The callback that will be triggered when the tooltip is hidden.
   *
   * @param dismissType the type of event that caused the tooltip to be hidden.
   */
  void onHide(@TooltipDismissType int dismissType);
}
