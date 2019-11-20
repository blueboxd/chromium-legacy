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

package com.google.android.libraries.feed.api.internal.actionmanager;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import java.util.List;

/** Allows Stream to notify the Feed library of actions taken */
public interface ActionManager {

  /**
   * Dismiss content for the content ID in the session, along with executing the provided stream
   * data operations on the session.
   *
   * @param contentIds The content IDs for the feature being dismissed. These are recorded and sent
   *     to the server in subsequent requests.
   * @param streamDataOperations Any stream data operations that should be applied to the session
   *     (e.g. removing a cluster when the content is removed)
   * @param sessionId The current session id
   */
  void dismissLocal(
      List<String> contentIds,
      List<StreamDataOperation> streamDataOperations,
      /*@Nullable*/ String sessionId);

  /**
   * Executes the provided stream data operations on the session.
   *
   * @param contentId The id for the content that triggered this action.
   * @param streamDataOperations Any stream data operations that should be applied to the session
   *     (e.g. removing a cluster when the content is removed)
   * @param sessionId The current session id
   */
  void dismiss(List<StreamDataOperation> streamDataOperations, /*@Nullable*/ String sessionId);

  /**
   * Issues a request to record a set of actions, with the consumer being called back with the
   * resulting {@link ConsistencyToken}.
   */
  void createAndUploadAction(String contentId, ActionPayload payload);

  /**
   * Issues a request to record a set of action and update the url with consistency token with the
   * consumer being called on the main thread with the resulting url.
   */
  void uploadAllActionsAndUpdateUrl(
      String url, String consistencyTokenQueryParamName, Consumer<String> consumer);
}
