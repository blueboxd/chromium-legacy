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

/** Events for scrolling. */
public interface ScrollEvents {
  /**
   * Notifies of the delta change of the scroll action and the ms timestamp of when the action
   * finished.
   */
  void onScrollEvent(int scrollAmount, long scrollEndTimestampMs);
}
