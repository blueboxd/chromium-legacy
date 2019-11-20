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

package com.google.android.libraries.feed.api.internal.requestmanager;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import java.util.Set;

/** Creates and issues upload action requests to the server. */
public interface ActionUploadRequestManager {

  /**
   * Issues a request to record a set of actions.
   *
   * <p>The provided {@code consumer} will be executed on a background thread.
   */
  void triggerUploadActions(
      Set<StreamUploadableAction> actions,
      ConsistencyToken token,
      Consumer<Result<ConsistencyToken>> consumer);
}
