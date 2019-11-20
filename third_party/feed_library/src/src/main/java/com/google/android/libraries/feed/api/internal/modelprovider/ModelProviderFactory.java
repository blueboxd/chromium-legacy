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

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/** Factory method for creating {@link ModelProvider} instances. */
public interface ModelProviderFactory {
  /**
   * Returns a new instance of a {@link ModelProvider} from an existing session. This session may
   * become INVALIDATED if the session was garbage collected by the Session Manager.
   */
  ModelProvider create(String sessionId, UiContext uiContext);

  /** Returns a new instance of a {@link ModelProvider} from $HEAD. */
  ModelProvider createNew(/*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext);

  ModelProvider createNew(
      /*@Nullable*/ ViewDepthProvider viewDepthProvider,
      /*@Nullable*/ Predicate<StreamStructure> filterPredicate,
      UiContext uiContext);
}
