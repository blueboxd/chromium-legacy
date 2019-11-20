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

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link FeedProcessScope}. */
@RunWith(RobolectricTestRunner.class)
public class FeedProcessScopeTest {

  @Mock private BasicLoggingApi basicLoggingApi;
  @Mock private NetworkClient networkClient;
  @Mock private ApplicationInfo applicationInfo;
  @Mock private TooltipSupportedApi tooltipSupportedApi;
  @Mock private ProtocolAdapter protocolAdapter;
  @Mock private RequestManager requestManager;
  @Mock private FeedSessionManager feedSessionManager;
  @Mock private Store store;
  @Mock private TaskQueue taskQueue;
  @Mock private AppLifecycleListener appLifecycleListener;
  @Mock private DebugBehavior debugBehavior;
  @Mock private ActionManager actionManager;
  @Mock private FeedKnownContent feedKnownContent;
  @Mock private FeedExtensionRegistry feedExtensionRegistry;
  @Mock private FeedObservable<FeedLifecycleListener> feedLifecycleListenerFeedObservable;

  private final Clock clock = new FakeClock();
  private final MainThreadRunner mainThreadRunner = new MainThreadRunner();
  private final ThreadUtils threadUtils = new ThreadUtils();
  private final TimingUtils timingUtils = new TimingUtils();
  private Configuration configuration = new Configuration.Builder().build();
  private ClearAllListener clearAllListener;

  @Before
  public void setUp() {
    initMocks(this);
    clearAllListener =
        new ClearAllListener(
            taskQueue, feedSessionManager, null, threadUtils, feedLifecycleListenerFeedObservable);
  }

  @Test
  public void testDestroy() throws Exception {
    FeedProcessScope processScope =
        new FeedProcessScope(
            basicLoggingApi,
            networkClient,
            protocolAdapter,
            requestManager,
            feedSessionManager,
            store,
            timingUtils,
            threadUtils,
            taskQueue,
            mainThreadRunner,
            appLifecycleListener,
            clock,
            debugBehavior,
            actionManager,
            configuration,
            feedKnownContent,
            feedExtensionRegistry,
            clearAllListener,
            tooltipSupportedApi,
            applicationInfo);
    processScope.onDestroy();
    verify(networkClient).close();
  }
}
