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

package com.google.android.libraries.feed.feedmodelprovider;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * Factory for creating instances of {@link ModelProviderFactory} using the {@link
 * FeedModelProvider}.
 */
public final class FeedModelProviderFactory implements ModelProviderFactory {
  private final FeedSessionManager feedSessionManager;
  private final ThreadUtils threadUtils;
  private final TimingUtils timingUtils;
  private final TaskQueue taskQueue;
  private final MainThreadRunner mainThreadRunner;
  private final Configuration config;
  private final BasicLoggingApi basicLoggingApi;

  public FeedModelProviderFactory(
      FeedSessionManager feedSessionManager,
      ThreadUtils threadUtils,
      TimingUtils timingUtils,
      TaskQueue taskQueue,
      MainThreadRunner mainThreadRunner,
      Configuration config,
      BasicLoggingApi basicLoggingApi) {
    this.feedSessionManager = feedSessionManager;
    this.threadUtils = threadUtils;
    this.timingUtils = timingUtils;
    this.taskQueue = taskQueue;
    this.mainThreadRunner = mainThreadRunner;
    this.config = config;
    this.basicLoggingApi = basicLoggingApi;
  }

  @Override
  public ModelProvider create(String sessionId, UiContext uiContext) {
    FeedModelProvider modelProvider =
        new FeedModelProvider(
            feedSessionManager,
            threadUtils,
            timingUtils,
            taskQueue,
            mainThreadRunner,
            null,
            config,
            basicLoggingApi);
    feedSessionManager.getExistingSession(sessionId, modelProvider, uiContext);
    return modelProvider;
  }

  @Override
  public ModelProvider createNew(
      /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext) {
    return createNew(viewDepthProvider, null, uiContext);
  }

  @Override
  public ModelProvider createNew(
      /*@Nullable*/ ViewDepthProvider viewDepthProvider,
      /*@Nullable*/ Predicate<StreamStructure> filterPredicate,
      UiContext uiContext) {
    FeedModelProvider modelProvider =
        new FeedModelProvider(
            feedSessionManager,
            threadUtils,
            timingUtils,
            taskQueue,
            mainThreadRunner,
            filterPredicate,
            config,
            basicLoggingApi);
    feedSessionManager.getNewSession(
        modelProvider, modelProvider.getViewDepthProvider(viewDepthProvider), uiContext);
    return modelProvider;
  }
}
