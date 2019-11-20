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

/** Object used to hold content data for logging events. */
public interface ElementLoggingData {
  /**
   * Returns the position of the content in the stream not accounting for header views. The top
   * being 0. Position does not change after initial layout. Specifically the position does not
   * update if dismisses/removes are performed.
   */
  int getPositionInStream();

  /**
   * Gets the time, in seconds from epoch, for when this content was made available on the device.
   * This could be the time for when this content was retrieved from the server or the time the data
   * was pushed to the device.
   */
  long getTimeContentBecameAvailable();

  /**
   * Gets the URI which represents this content. This will normally be the URI which this content
   * links to.
   */
  String getRepresentationUri();
}
