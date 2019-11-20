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

package com.google.android.libraries.feed.feedapplifecyclelistener;

import android.support.annotation.StringDef;

/**
 * Internal interface used to register Feed components with the {@link FeedAppLifecycleListener},
 * which is used by consuming hosts to communicate app lifecycle events to the Feed Library.
 */
public interface FeedLifecycleListener {
  /** The types of lifecycle events. */
  @StringDef({
    LifecycleEvent.ENTER_FOREGROUND,
    LifecycleEvent.ENTER_BACKGROUND,
    LifecycleEvent.CLEAR_ALL,
    LifecycleEvent.CLEAR_ALL_WITH_REFRESH,
    LifecycleEvent.INITIALIZE
  })
  @interface LifecycleEvent {
    String ENTER_FOREGROUND = "foreground";
    String ENTER_BACKGROUND = "background";
    String CLEAR_ALL = "clearAll";
    String CLEAR_ALL_WITH_REFRESH = "clearAllWithRefresh";
    String INITIALIZE = "initialize";
  }

  void onLifecycleEvent(@LifecycleEvent String event);
}
