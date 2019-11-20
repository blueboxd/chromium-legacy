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

/** Object used to hold content data for logging events. */
public interface ContentLoggingData extends ElementLoggingData {

  /** Gets the time, in seconds from epoch, for when the content was published/made available. */
  long getPublishedTimeSeconds();

  /** Gets the score which was given to content from NowStream. */
  float getScore();

  /**
   * Gets offline availability status.
   *
   * <p>Note: The offline availability status for one piece of content can change. When this is
   * logged, it should be logged with the offline status as of logging time. This means that one
   * piece of content can emit multiple {@link ContentLoggingData} instances which are not equal to
   * each other.
   */
  boolean isAvailableOffline();
}
