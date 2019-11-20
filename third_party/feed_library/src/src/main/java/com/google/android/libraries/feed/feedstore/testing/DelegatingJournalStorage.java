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

package com.google.android.libraries.feed.feedstore.testing;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalStorage;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.List;

/**
 * Delegate class allowing spying on JournalStorageDirect delegates implementing both of the storage
 * interfaces.
 */
public class DelegatingJournalStorage implements JournalStorage, JournalStorageDirect {
  private final JournalStorageDirect delegate;

  public DelegatingJournalStorage(JournalStorageDirect delegate) {
    this.delegate = delegate;
  }

  @Override
  public void read(String journalName, Consumer<Result<List<byte[]>>> consumer) {
    consumer.accept(delegate.read(journalName));
  }

  @Override
  public Result<List<byte[]>> read(String journalName) {
    return delegate.read(journalName);
  }

  @Override
  public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
    consumer.accept(delegate.commit(mutation));
  }

  @Override
  public CommitResult commit(JournalMutation mutation) {
    return delegate.commit(mutation);
  }

  @Override
  public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
    consumer.accept(delegate.exists(journalName));
  }

  @Override
  public Result<Boolean> exists(String journalName) {
    return delegate.exists(journalName);
  }

  @Override
  public void getAllJournals(Consumer<Result<List<String>>> consumer) {
    consumer.accept(delegate.getAllJournals());
  }

  @Override
  public Result<List<String>> getAllJournals() {
    return delegate.getAllJournals();
  }

  @Override
  public void deleteAll(Consumer<CommitResult> consumer) {
    consumer.accept(delegate.deleteAll());
  }

  @Override
  public CommitResult deleteAll() {
    return delegate.deleteAll();
  }
}
