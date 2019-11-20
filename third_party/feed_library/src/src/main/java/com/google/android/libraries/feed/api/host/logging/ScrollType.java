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

package com.google.android.libraries.feed.api.host.logging;

import android.support.annotation.IntDef;

/** IntDef representing the different types of scrolls. */
@IntDef({ScrollType.UNKNOWN, ScrollType.STREAM_SCROLL, ScrollType.NEXT_VALUE})
public @interface ScrollType {
  // Type of scroll that occurs
  int UNKNOWN = 0;
  // Scroll the stream of cards
  int STREAM_SCROLL = 1;
  int NEXT_VALUE = 2;
}
