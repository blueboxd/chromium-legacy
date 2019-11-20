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

package com.google.android.libraries.feed.feedstore.internal;

import static com.google.android.libraries.feed.feedstore.internal.FeedStoreConstants.SESSION_NAME_PREFIX;

import java.util.UUID;

/** Helper class for shared FeedStore methods */
public final class FeedStoreHelper {
  /** Get a new, unique stream session id. */
  String getNewStreamSessionId() {
    return SESSION_NAME_PREFIX + UUID.randomUUID();
  }
}
