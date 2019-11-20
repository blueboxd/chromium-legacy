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

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

/**
 * Mutation which will batch a set of atomic changes to the Model maintained by the ModelProvider.
 * Order is important, things will be stored in the order they are added to a parent feature.
 */
public interface ModelMutation {
  /** Add the structural information to the model as a child */
  ModelMutation addChild(StreamStructure streamStructure);

  /** Remove the model child defined by the structural information */
  ModelMutation removeChild(StreamStructure streamStructure);

  /** Content for a child was updated. */
  ModelMutation updateChild(StreamStructure streamStructure);

  /**
   * Set the MutationContext used to create the ModelMutation. This may contain the {@link
   * StreamToken} which represents the source of the response.
   */
  ModelMutation setMutationContext(MutationContext mutationContext);

  /** Set the session id of the session backing this ModelProvider. */
  ModelMutation setSessionId(String sessionId);

  /** Indicates that the SessionManager has cached the payload bindings. */
  ModelMutation hasCachedBindings(boolean cachedBindings);

  /** Commits the pending changes to the store. */
  void commit();
}
