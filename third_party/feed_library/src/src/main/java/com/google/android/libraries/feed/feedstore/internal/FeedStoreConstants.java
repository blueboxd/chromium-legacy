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

/** Group of constants for use across the {@link FeedStore} class and its internal classes */
public final class FeedStoreConstants {

  /** Can't be instantiated */
  private FeedStoreConstants() {}

  /** Used to prefix session IDs */
  public static final String SESSION_NAME_PREFIX = "_session:";

  /** Key used to prefix Feed shared states keys in {@link ContentStorage} */
  public static final String SHARED_STATE_PREFIX = "ss::";
  
  /** Key used to prefix Feed semantic properties keys in {@link ContentStorage} */
  public static final String SEMANTIC_PROPERTIES_PREFIX = "sp::";

  /** Key used to prefix Feed uploadableAction keys in {@link ContentStorage} */
  public static final String UPLOADABLE_ACTION_PREFIX = "ua::";

  /** The name of the journal used to store dismiss actions */
  public static final String DISMISS_ACTION_JOURNAL = "action-dismiss";
}
