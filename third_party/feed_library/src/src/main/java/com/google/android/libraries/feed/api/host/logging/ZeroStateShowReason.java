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

package com.google.android.libraries.feed.api.host.logging;

import android.support.annotation.IntDef;

/**
 * The reason a zero state is shown.
 *
 * <p>When adding new values, the value of {@link ZeroStateShowReason#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link ZeroStateShowReason#NEXT_VALUE} should not be changed,
 * and those values should not be reused.
 */
@IntDef({
  ZeroStateShowReason.ERROR,
  ZeroStateShowReason.NO_CONTENT,
  ZeroStateShowReason.CONTENT_DISMISSED,
  ZeroStateShowReason.NO_CONTENT_FROM_CONTINUATION_TOKEN,
  ZeroStateShowReason.NEXT_VALUE,
})
public @interface ZeroStateShowReason {

  // Indicates the zero state was shown due to an error. This would most commonly happen if a
  // request fails.
  int ERROR = 0;

  // Indicates the zero state was shown because no content is available. This can happen if the host
  // does not schedule a refresh when the app is opened.
  int NO_CONTENT = 1;

  // Indicates the zero state was shown because all content, including any tokens, were dismissed.
  int CONTENT_DISMISSED = 2;

  // Indicates that the only content showing was a continuation token, and that token completed with
  // no additional content, resulting in no content showing. This shouldn't occur, as a token should
  // at least return another token.
  int NO_CONTENT_FROM_CONTINUATION_TOKEN = 3;

  // The next value that should be used when adding additional values to the IntDef.
  int NEXT_VALUE = 4;
}
