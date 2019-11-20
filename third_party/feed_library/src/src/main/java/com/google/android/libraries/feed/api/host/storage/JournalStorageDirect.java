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

/** Define a version of the {@link JournalStorage} which runs synchronously. */
public interface JournalStorageDirect {

  /**
   * Reads the journal and a returns the contents.
   *
   * <p>Reads on journals that do not exist will fulfill with an empty list.
   */
  Result<List<byte[]>> read(String journalName);

  /**
   * Commits the operations in {@link JournalMutation} in order and reports the {@link
   * CommitResult}. If all the operations succeed returns a success result, otherwise reports a
   * failure.
   *
   * <p>This operation is not guaranteed to be atomic.
   */
  CommitResult commit(JournalMutation mutation);

  /** Determines whether a journal exists. */
  Result<Boolean> exists(String journalName);

  /** Retrieve a list of all current journals */
  Result<List<String>> getAllJournals();

  /** Delete all journals. */
  CommitResult deleteAll();
}
