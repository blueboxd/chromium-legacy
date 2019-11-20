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

package com.google.android.libraries.feed.feedactionparser.internal;

import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.Type;

/** Utility class to convert a {@link Type} to {@link ActionType}. */
public final class ActionTypeConverter {
  private ActionTypeConverter() {}

  @ActionType
  public static int convert(Type type) {
    // LINT.IfChange
    switch (type) {
      case OPEN_URL:
        return ActionType.OPEN_URL;
      case OPEN_URL_INCOGNITO:
        return ActionType.OPEN_URL_INCOGNITO;
      case OPEN_URL_NEW_TAB:
        return ActionType.OPEN_URL_NEW_TAB;
      case OPEN_URL_NEW_WINDOW:
        return ActionType.OPEN_URL_NEW_WINDOW;
      case DOWNLOAD:
        return ActionType.DOWNLOAD;
      case LEARN_MORE:
        return ActionType.LEARN_MORE;
      default:
        return ActionType.UNKNOWN;
    }
    // LINT.ThenChange
  }
}
