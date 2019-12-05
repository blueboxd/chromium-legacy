// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.scope;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link FeedProcessScope}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedProcessScopeTest {
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private NetworkClient mNetworkClient;
    @Mock
    private ApplicationInfo mApplicationInfo;
    @Mock
    private TooltipSupportedApi mTooltipSupportedApi;
    @Mock
    private ProtocolAdapter mProtocolAdapter;
    @Mock
    private RequestManager mRequestManager;
    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Store mStore;
    @Mock
    private TaskQueue mTaskQueue;
    @Mock
    private AppLifecycleListener mAppLifecycleListener;
    @Mock
    private DebugBehavior mDebugBehavior;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private FeedKnownContent mFeedKnownContent;
    @Mock
    private FeedExtensionRegistry mFeedExtensionRegistry;
    @Mock
    private FeedObservable<FeedLifecycleListener> mFeedLifecycleListenerFeedObservable;

    private final Clock mClock = new FakeClock();
    private final MainThreadRunner mMainThreadRunner = new MainThreadRunner();
    private final ThreadUtils mThreadUtils = new ThreadUtils();
    private final TimingUtils mTimingUtils = new TimingUtils();
    private Configuration mConfiguration = new Configuration.Builder().build();
    private ClearAllListener mClearAllListener;

    @Before
    public void setUp() {
        initMocks(this);
        mClearAllListener = new ClearAllListener(mTaskQueue, mFeedSessionManager, null,
                mThreadUtils, mFeedLifecycleListenerFeedObservable);
    }

    @Test
    public void testDestroy() throws Exception {
        FeedProcessScope processScope = new FeedProcessScope(mBasicLoggingApi, mNetworkClient,
                mProtocolAdapter, mRequestManager, mFeedSessionManager, mStore, mTimingUtils,
                mThreadUtils, mTaskQueue, mMainThreadRunner, mAppLifecycleListener, mClock,
                mDebugBehavior, mActionManager, mConfiguration, mFeedKnownContent,
                mFeedExtensionRegistry, mClearAllListener, mTooltipSupportedApi, mApplicationInfo);
        processScope.onDestroy();
        verify(mNetworkClient).close();
    }
}
