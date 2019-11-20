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

package com.google.android.libraries.feed.feedstore;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadCaller;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * An implementation of {@link ContentStorageDirect} which converts {@link ContentStorage} to a
 * synchronized implementation. This acts as a wrapper class over ContentStorage. It will provide a
 * consumer calling on the main thread and waiting on a Future to complete to return the consumer
 * results.
 */
public final class ContentStorageDirectImpl extends MainThreadCaller
    implements ContentStorageDirect {
  private static final String LOCATION = "ContentStorage.";
  private final ContentStorage contentStorage;

  public ContentStorageDirectImpl(
      ContentStorage contentStorage, MainThreadRunner mainThreadRunner) {
    super(mainThreadRunner);
    this.contentStorage = contentStorage;
  }

  @Override
  public Result<Map<String, byte[]>> get(List<String> keys) {
    if (keys.isEmpty()) {
      return Result.success(Collections.emptyMap());
    }

    return mainThreadCaller(
        LOCATION + "get", (consumer) -> contentStorage.get(keys, consumer), Result.failure());
  }

  @Override
  public Result<Map<String, byte[]>> getAll(String prefix) {
    return mainThreadCaller(
        LOCATION + "getAll",
        (consumer) -> contentStorage.getAll(prefix, consumer),
        Result.failure());
  }

  @Override
  public CommitResult commit(ContentMutation mutation) {
    return mainThreadCaller(
        LOCATION + "commit",
        (consumer) -> contentStorage.commit(mutation, consumer),
        CommitResult.FAILURE);
  }

  @Override
  public Result<List<String>> getAllKeys() {
    return mainThreadCaller(LOCATION + "getAllKeys", contentStorage::getAllKeys, Result.failure());
  }
}
