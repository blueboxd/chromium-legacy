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

package com.google.android.libraries.feed.api.host.storage;

import com.google.android.libraries.feed.common.Result;
import java.util.List;
import java.util.Map;

/** A content storage API which is a synchronous implementation of {@link ContentStorage}. */
public interface ContentStorageDirect {

  /**
   * Requests the value for multiple keys. If a key does not have a value, it will not be included
   * in the map.
   */
  Result<Map<String, byte[]>> get(List<String> keys);

  /** Requests all key/value pairs from storage with a matching key prefix. */
  Result<Map<String, byte[]>> getAll(String prefix);

  /**
   * Commits the operations in the {@link ContentMutation} in order.
   *
   * <p>This operation is not guaranteed to be atomic. In the event of a failure, processing is
   * halted immediately, so the database may be left in an invalid state. Should this occur, Feed
   * behavior is undefined. Currently the plan is to wipe out existing data and start over.
   */
  CommitResult commit(ContentMutation mutation);

  /** Fetch all keys currently present in the content storage */
  Result<List<String>> getAllKeys();
}
