// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo.BuildType;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.stream.StreamFactory;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link StreamScopeBuilder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamScopeBuilderTest {
    @BuildType
    private static final int BUILD_TYPE = BuildType.ALPHA;
    public static final DebugBehavior DEBUG_BEHAVIOR = DebugBehavior.VERBOSE;

    // Mocks for required fields
    @Mock
    private ActionApi mActionApi;
    @Mock
    private ImageLoaderApi mImageLoaderApi;
    @Mock
    private TooltipSupportedApi mTooltipSupportedApi;

    // Mocks for optional fields
    @Mock
    private ProtocolAdapter mProtocolAdapter;
    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Stream mStream;
    @Mock
    private StreamConfiguration mStreamConfiguration;
    @Mock
    private CardConfiguration mCardConfiguration;
    @Mock
    private ModelProviderFactory mModelProviderFactory;
    @Mock
    private CustomElementProvider mCustomElementProvider;
    @Mock
    private HostBindingProvider mHostBindingProvider;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private Configuration mConfig;
    @Mock
    private SnackbarApi mSnackbarApi;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private OfflineIndicatorApi mOfflineIndicatorApi;
    @Mock
    private FeedKnownContent mFeedKnownContent;
    @Mock
    private TaskQueue mTaskQueue;
    @Mock
    private TooltipApi mTooltipApi;
    @Mock
    private FeedExtensionRegistry mFeedExtensionRegistry;

    private Context mContext;
    private MainThreadRunner mMainThreadRunner;
    private ThreadUtils mThreadUtils;
    private TimingUtils mTimingUtils;
    private Clock mClock;
    private ApplicationInfo mApplicationInfo;
    private StreamFactory mStreamFactory;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mMainThreadRunner = new MainThreadRunner();
        mThreadUtils = new ThreadUtils();
        mTimingUtils = new TimingUtils();
        mClock = new FakeClock();
        mApplicationInfo = new ApplicationInfo.Builder(mContext).setBuildType(BUILD_TYPE).build();
        when(mConfig.getValueOrDefault(
                     eq(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS), anyLong()))
                .thenReturn(1000L);
        when(mConfig.getValueOrDefault(eq(ConfigKey.FADE_IMAGE_THRESHOLD_MS), anyLong()))
                .thenReturn(80L);
    }

    @Test
    public void testBasicBuild() {
        StreamScope streamScope = new StreamScopeBuilder(mContext, mActionApi, mImageLoaderApi,
                mProtocolAdapter, mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue,
                mMainThreadRunner, mClock, DEBUG_BEHAVIOR, mStreamConfiguration, mCardConfiguration,
                mActionManager, mConfig, mSnackbarApi, mBasicLoggingApi, mOfflineIndicatorApi,
                mFeedKnownContent, mTooltipApi, mTooltipSupportedApi, mApplicationInfo,
                mFeedExtensionRegistry)
                                          .build();
        assertThat(streamScope.getStream()).isNotNull();
        assertThat(streamScope.getModelProviderFactory()).isNotNull();
    }

    @Test
    public void testComplexBuild() {
        StreamScope streamScope = new StreamScopeBuilder(mContext, mActionApi, mImageLoaderApi,
                mProtocolAdapter, mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue,
                mMainThreadRunner, mClock, DEBUG_BEHAVIOR, mStreamConfiguration, mCardConfiguration,
                mActionManager, mConfig, mSnackbarApi, mBasicLoggingApi, mOfflineIndicatorApi,
                mFeedKnownContent, mTooltipApi, mTooltipSupportedApi, mApplicationInfo,
                mFeedExtensionRegistry)
                                          .setModelProviderFactory(mModelProviderFactory)
                                          .setCustomElementProvider(mCustomElementProvider)
                                          .setHostBindingProvider(new HostBindingProvider())
                                          .build();
        assertThat(streamScope.getStream()).isNotNull();
        assertThat(streamScope.getModelProviderFactory()).isEqualTo(mModelProviderFactory);
    }

    @Test
    public void testStreamFactoryBuild() {
        setupStreamFactory(mStream);

        StreamScope streamScope = new StreamScopeBuilder(mContext, mActionApi, mImageLoaderApi,
                mProtocolAdapter, mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue,
                mMainThreadRunner, mClock, DEBUG_BEHAVIOR, mStreamConfiguration, mCardConfiguration,
                mActionManager, mConfig, mSnackbarApi, mBasicLoggingApi, mOfflineIndicatorApi,
                mFeedKnownContent, mTooltipApi, mTooltipSupportedApi, mApplicationInfo,
                mFeedExtensionRegistry)
                                          .setStreamFactory(mStreamFactory)
                                          .setCustomElementProvider(mCustomElementProvider)
                                          .setHostBindingProvider(mHostBindingProvider)
                                          .build();
        assertThat(streamScope.getStream()).isEqualTo(mStream);
    }

    private void setupStreamFactory(Stream streamToReturn) {
        mStreamFactory = (actionParserFactory, context, buildType, cardConfiguration,
                imageLoaderApi, customElementProvider, debugBehavior, clock, modelProviderFactory,
                hostBindingProvider, offlineIndicatorApi, configuration, actionApi, actionManager,
                snackbarApi, streamConfiguration, feedExtensionRegistry, basicLoggingApi,
                mainThreadRunner, isBackgroundDark, tooltipApi, threadUtils,
                knownContentApi) -> streamToReturn;
    }
}
