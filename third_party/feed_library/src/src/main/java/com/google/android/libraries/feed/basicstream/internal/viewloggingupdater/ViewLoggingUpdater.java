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

package com.google.android.libraries.feed.basicstream.internal.viewloggingupdater;

import com.google.android.libraries.feed.common.feedobservable.FeedObservable;

/** {@link FeedObservable} to reset whether views have been logged as visible. */
public class ViewLoggingUpdater extends FeedObservable<ResettableOneShotVisibilityLoggingListener> {

  /** Resets views logging state to allow them to be re-logged as visible. */
  public void resetViewTracking() {
    for (ResettableOneShotVisibilityLoggingListener loggingListener : observers) {
      loggingListener.reset();
    }
  }
}
