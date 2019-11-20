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

package com.google.android.libraries.feed.api.internal.actionparser;

import android.support.annotation.IntDef;

/** Possible action types. */
@IntDef({
  ActionSource.UNKNOWN,
  ActionSource.VIEW,
  ActionSource.CLICK,
  ActionSource.LONG_CLICK,
  ActionSource.SWIPE,
  ActionSource.CONTEXT_MENU
})
public @interface ActionSource {
  int UNKNOWN = 0;
  /** View action */
  int VIEW = 1;

  /** Click action */
  int CLICK = 2;

  /** Long click action */
  int LONG_CLICK = 3;

  /* Swipe action */
  int SWIPE = 4;

  /* Action performed from context menu*/
  int CONTEXT_MENU = 5;
}
