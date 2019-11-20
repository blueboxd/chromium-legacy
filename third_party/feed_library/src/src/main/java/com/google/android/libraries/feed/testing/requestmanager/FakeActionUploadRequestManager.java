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

package com.google.android.libraries.feed.testing.requestmanager;

import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import java.util.HashSet;
import java.util.Set;

/** Fake implements of {@link ActionUploadRequestManager}. */
public final class FakeActionUploadRequestManager implements ActionUploadRequestManager {
  private final FakeThreadUtils fakeThreadUtils;
  private Result<ConsistencyToken> result = Result.success(ConsistencyToken.getDefaultInstance());
  /*@Nullable*/ private Set<StreamUploadableAction> actions = null;

  public FakeActionUploadRequestManager(FakeThreadUtils fakeThreadUtils) {
    this.fakeThreadUtils = fakeThreadUtils;
  }

  @Override
  public void triggerUploadActions(
      Set<StreamUploadableAction> actions,
      ConsistencyToken token,
      Consumer<Result<ConsistencyToken>> consumer) {
    this.actions = actions;
    boolean policy = fakeThreadUtils.enforceMainThread(false);
    try {
      consumer.accept(result);
    } finally {
      fakeThreadUtils.enforceMainThread(policy);
    }
  }

  /**
   * Sets the result to return from triggerUploadActions. If unset will use {@code
   * Result.success(ConsistencyToken.getDefaultInstance())}.
   */
  public FakeActionUploadRequestManager setResult(Result<ConsistencyToken> result) {
    this.result = result;
    return this;
  }

  /** Returns the last set of actions sent to triggerUploadActions. */
  public Set<StreamUploadableAction> getLatestActions() {
    if (actions == null) {
      return new HashSet<>();
    }

    return actions;
  }
}
