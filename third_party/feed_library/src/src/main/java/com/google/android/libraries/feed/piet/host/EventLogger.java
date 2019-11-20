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

package com.google.android.libraries.feed.piet.host;

import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import java.util.List;

/** Allows Piet to report events to the host. */
public interface EventLogger {

  /**
   * Tells the host each {@link ErrorCode} that was raised during the binding of a Frame. This list
   * can contain duplicates.
   */
  void logEvents(List<ErrorCode> errorCodes);
}
