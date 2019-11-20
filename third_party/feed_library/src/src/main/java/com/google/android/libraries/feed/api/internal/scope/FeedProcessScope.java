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

package com.google.android.libraries.feed.api.internal.scope;

import android.content.Context;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.api.client.scope.StreamScopeBuilder;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;

/**
 * Per-process instance of the feed library.
 *
 * <p>It's the host's responsibility to make sure there's only one instance of this per process, per
 * user.
 */
public final class FeedProcessScope implements ProcessScope {
  private static final String TAG = "FeedProcessScope";

  private final NetworkClient networkClient;
  private final ProtocolAdapter protocolAdapter;
  private final RequestManager requestManager;
  private final FeedSessionManager feedSessionManager;
  private final Store store;
  private final TimingUtils timingUtils;
  private final ThreadUtils threadUtils;
  private final TaskQueue taskQueue;
  private final MainThreadRunner mainThreadRunner;
  private final AppLifecycleListener appLifecycleListener;
  private final Clock clock;
  private final DebugBehavior debugBehavior;
  private final ActionManager actionManager;
  private final Configuration configuration;
  private final FeedKnownContent feedKnownContent;
  private final FeedExtensionRegistry feedExtensionRegistry;
  private final ClearAllListener clearAllListener;
  private final BasicLoggingApi basicLoggingApi;
  private final TooltipSupportedApi tooltipSupportedApi;
  private final ApplicationInfo applicationInfo;

  /** Created through the {@link Builder}. */
  public FeedProcessScope(
      BasicLoggingApi basicLoggingApi,
      NetworkClient networkClient,
      ProtocolAdapter protocolAdapter,
      RequestManager requestManager,
      FeedSessionManager feedSessionManager,
      Store store,
      TimingUtils timingUtils,
      ThreadUtils threadUtils,
      TaskQueue taskQueue,
      MainThreadRunner mainThreadRunner,
      AppLifecycleListener appLifecycleListener,
      Clock clock,
      DebugBehavior debugBehavior,
      ActionManager actionManager,
      Configuration configuration,
      FeedKnownContent feedKnownContent,
      FeedExtensionRegistry feedExtensionRegistry,
      ClearAllListener clearAllListener,
      TooltipSupportedApi tooltipSupportedApi,
      ApplicationInfo applicationInfo) {
    this.basicLoggingApi = basicLoggingApi;
    this.networkClient = networkClient;
    this.protocolAdapter = protocolAdapter;
    this.requestManager = requestManager;
    this.feedSessionManager = feedSessionManager;
    this.store = store;
    this.timingUtils = timingUtils;
    this.threadUtils = threadUtils;
    this.taskQueue = taskQueue;
    this.mainThreadRunner = mainThreadRunner;
    this.appLifecycleListener = appLifecycleListener;
    this.clock = clock;
    this.debugBehavior = debugBehavior;
    this.actionManager = actionManager;
    this.configuration = configuration;
    this.feedKnownContent = feedKnownContent;
    this.feedExtensionRegistry = feedExtensionRegistry;
    this.clearAllListener = clearAllListener;
    this.tooltipSupportedApi = tooltipSupportedApi;
    this.applicationInfo = applicationInfo;
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    if (protocolAdapter instanceof Dumpable) {
      dumper.dump((Dumpable) protocolAdapter);
    }
    dumper.dump(timingUtils);
    if (feedSessionManager instanceof Dumpable) {
      dumper.dump((Dumpable) feedSessionManager);
    }
    if (store instanceof Dumpable) {
      dumper.dump((Dumpable) store);
    }
    dumper.dump(clearAllListener);
  }

  @Override
  public void onDestroy() {
    try {
      Logger.i(TAG, "FeedProcessScope onDestroy called");
      networkClient.close();
      taskQueue.reset();
      taskQueue.completeReset();
    } catch (Exception ignored) {
      // Ignore exception when closing.
    }
  }

  @Deprecated
  public ProtocolAdapter getProtocolAdapter() {
    return protocolAdapter;
  }


  public RequestManager getRequestManager() {
    return requestManager;
  }

  @Deprecated
  public TimingUtils getTimingUtils() {
    return timingUtils;
  }

  public TaskQueue getTaskQueue() {
    return taskQueue;
  }

  public AppLifecycleListener getAppLifecycleListener() {
    return appLifecycleListener;
  }

  @Deprecated
  public ActionManager getActionManager() {
    return actionManager;
  }

  public KnownContent getKnownContent() {
    return feedKnownContent;
  }

  @Deprecated
  public FeedExtensionRegistry getFeedExtensionRegistry() {
    return feedExtensionRegistry;
  }

  /**
   * Return a {@link Builder} to create a FeedProcessScope
   *
   * <p>This is called by hosts so it must be public
   */
  @Override
  public StreamScopeBuilder createStreamScopeBuilder(
      Context context,
      ImageLoaderApi imageLoaderApi,
      ActionApi actionApi,
      StreamConfiguration streamConfiguration,
      CardConfiguration cardConfiguration,
      SnackbarApi snackbarApi,
      OfflineIndicatorApi offlineIndicatorApi,
      TooltipApi tooltipApi) {
    return new StreamScopeBuilder(
        context,
        actionApi,
        imageLoaderApi,
        protocolAdapter,
        feedSessionManager,
        threadUtils,
        timingUtils,
        taskQueue,
        mainThreadRunner,
        clock,
        debugBehavior,
        streamConfiguration,
        cardConfiguration,
        actionManager,
        configuration,
        snackbarApi,
        basicLoggingApi,
        offlineIndicatorApi,
        feedKnownContent,
        tooltipApi,
        tooltipSupportedApi,
        applicationInfo,
        feedExtensionRegistry);
  }
}
