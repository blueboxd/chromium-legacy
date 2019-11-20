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

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import java.util.List;

/** A {@link Session} which supports initialization from $HEAD. */
public interface InitializableSession extends Session {

  /**
   * Bind a ModelProvider to an existing, unbound session. If {@code null} is passed, this will
   * unbind the Session, removing all references to the ModelProvider.
   */
  void bindModelProvider(
      /*@Nullable*/ ModelProvider modelProvider, /*@Nullable*/ ViewDepthProvider viewDepthProvider);

  /** Set the session id. */
  void setSessionId(String sessionId);

  /** Called to initialize the session from $HEAD */
  void populateModelProvider(
      List<StreamStructure> session,
      boolean cachedBindings,
      boolean legacyHeadContent,
      UiContext uiContext);
}
