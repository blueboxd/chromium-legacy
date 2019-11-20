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
package com.google.android.libraries.feed.sharedstream.scroll;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ScrollType;

/** */
public class ScrollLogger {
  private final BasicLoggingApi api;
  // We don't want to log scrolls that are tiny since the user probably didn't mean to actually
  // scroll.
  private static final int SCROLL_TOLERANCE = 10;

  public ScrollLogger(BasicLoggingApi api) {
    this.api = api;
  }
  /** Handles logging of scrolling. */
  public void handleScroll(@ScrollType int scrollType, int scrollAmount) {
    if (Math.abs(scrollAmount) <= SCROLL_TOLERANCE) {
      return;
    }
    api.onScroll(scrollType, scrollAmount);
  }
}
