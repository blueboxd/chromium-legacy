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

package com.google.android.libraries.feed.api.internal.modelprovider;

/** Observes for when the Token fetch completes, producing a new cursor. */
public interface TokenCompletedObserver {
  /** Called when the token processing has completed. */
  void onTokenCompleted(TokenCompleted tokenCompleted);

  /**
   * This is called in the event of an error. For example, if we are making a pagination request and
   * it fails due to network connectivity issues, this event will indicate the error.
   */
  void onError(ModelError modelError);
}
