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
 * IntDef representing the different results of requesting sessions.
 *
 * <p>When adding new values, the value of {@link SessionEvent#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link SessionEvent#NEXT_VALUE} should not be changed, and
 * those values should not be reused.
 */
@IntDef({
  SessionEvent.STARTED,
  SessionEvent.FINISHED_IMMEDIATELY,
  SessionEvent.ERROR,
  SessionEvent.USER_ABANDONED,
  SessionEvent.NEXT_VALUE,
})
public @interface SessionEvent {

  // Indicates that a session was successfully started when requested.
  int STARTED = 0;

  // Indicates that a session was immediately finished when requested.
  int FINISHED_IMMEDIATELY = 1;

  // Indicates that a session failed when requested.
  int ERROR = 2;

  // Indicates the session requested was abandoned by the user taking action.
  // Note: some of these events will be dropped, as they will be reported in
  // onDestroy(), which may not be called in all instances.
  int USER_ABANDONED = 3;

  // The next value that should be used when adding additional values to the IntDef.
  int NEXT_VALUE = 4;
}
