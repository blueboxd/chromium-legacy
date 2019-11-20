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
 * The reason a request is being made.
 *
 * <p>When adding new values, the value of {@link RequestReason#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link RequestReason#NEXT_VALUE} should not be changed, and
 * those values should not be reused.
 */
@IntDef({
  RequestReason.UNKNOWN,
  RequestReason.ZERO_STATE,
  RequestReason.HOST_REQUESTED,
  RequestReason.OPEN_WITH_CONTENT,
  RequestReason.MANUAL_CONTINUATION,
  RequestReason.AUTOMATIC_CONTINUATION,
  RequestReason.OPEN_WITHOUT_CONTENT,
  RequestReason.CLEAR_ALL,
  RequestReason.NEXT_VALUE,
})
public @interface RequestReason {

  // An unknown refresh reason.
  int UNKNOWN = 0;

  // Refresh triggered because the user manually hit the refresh button from the
  // zero-state.
  int ZERO_STATE = 1;

  // Refresh triggered because the host requested a refresh.
  int HOST_REQUESTED = 2;

  // Refresh triggered because there was stale content. The stale content was
  // shown while a background refresh would occur, which would be appended below
  // content the user had seen.
  int OPEN_WITH_CONTENT = 3;

  // Refresh triggered because the user tapped on a 'More' button.
  int MANUAL_CONTINUATION = 4;

  // Refresh triggered via automatically consuming a continuation token, without
  // showing the user a 'More' button.
  int AUTOMATIC_CONTINUATION = 5;

  // Refresh made when the Stream starts up and no content is showing.
  int OPEN_WITHOUT_CONTENT = 6;

  // Refresh made because the host requested a clear all.
  int CLEAR_ALL = 7;

  // The next value that should be used when adding additional values to the IntDef.
  int NEXT_VALUE = 8;
}
