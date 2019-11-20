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

import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;

/** Utility class to convert a {@link ActionType} to {@link ActionSource}. */
public final class ActionSourceConverter {
  private ActionSourceConverter() {}

  @ActionSource
  public static int convertPietAction(@ActionType int type) {
    switch (type) {
      case ActionType.VIEW:
        return ActionSource.VIEW;
      case ActionType.CLICK:
        return ActionSource.CLICK;
      case ActionType.LONG_CLICK:
        return ActionSource.LONG_CLICK;
      default:
        return ActionSource.UNKNOWN;
    }
  }
}
