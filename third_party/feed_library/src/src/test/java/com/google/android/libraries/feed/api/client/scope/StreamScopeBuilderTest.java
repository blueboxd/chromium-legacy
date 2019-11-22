// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.scope;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo.BuildType;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.stream.StreamFactory;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link StreamScopeBuilder}. */
@RunWith(RobolectricTestRunner.class)
public class StreamScopeBuilderTest {
    @BuildType
    private static final int BUILD_TYPE = BuildType.ALPHA;
    public static final DebugBehavior DEBUG_BEHAVIOR = DebugBehavior.VERBOSE;

    // Mocks for required fields
    @Mock
    private ActionApi actionApi;
    @Mock
    private ImageLoaderApi imageLoaderApi;
    @Mock
    private TooltipSupportedApi tooltipSupportedApi;

    // Mocks for optional fields
    @Mock
    private ProtocolAdapter protocolAdapter;
    @Mock
    private FeedSessionManager feedSessionManager;
    @Mock
    private Stream stream;
    @Mock
    private StreamConfiguration streamConfiguration;
    @Mock
    private CardConfiguration cardConfiguration;
    @Mock
    private ModelProviderFactory modelProviderFactory;
    @Mock
    private CustomElementProvider customElementProvider;
    @Mock
    private HostBindingProvider hostBindingProvider;
    @Mock
    private ActionManager actionManager;
    @Mock
    private Configuration config;
    @Mock
    private SnackbarApi snackbarApi;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private OfflineIndicatorApi offlineIndicatorApi;
    @Mock
    private FeedKnownContent feedKnownContent;
    @Mock
    private TaskQueue taskQueue;
    @Mock
    private TooltipApi tooltipApi;
    @Mock
    private FeedExtensionRegistry feedExtensionRegistry;

    private Context context;
    private MainThreadRunner mainThreadRunner;
    private ThreadUtils threadUtils;
    private TimingUtils timingUtils;
    private Clock clock;
    private ApplicationInfo applicationInfo;
    private StreamFactory streamFactory;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        mainThreadRunner = new MainThreadRunner();
        threadUtils = new ThreadUtils();
        timingUtils = new TimingUtils();
        clock = new FakeClock();
        applicationInfo = new ApplicationInfo.Builder(context).setBuildType(BUILD_TYPE).build();
        when(config.getValueOrDefault(
                     eq(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS), anyLong()))
                .thenReturn(1000L);
        when(config.getValueOrDefault(eq(ConfigKey.FADE_IMAGE_THRESHOLD_MS), anyLong()))
                .thenReturn(80L);
    }

    @Test
    public void testBasicBuild() {
        StreamScope streamScope = new StreamScopeBuilder(context, actionApi, imageLoaderApi,
                protocolAdapter, feedSessionManager, threadUtils, timingUtils, taskQueue,
                mainThreadRunner, clock, DEBUG_BEHAVIOR, streamConfiguration, cardConfiguration,
                actionManager, config, snackbarApi, basicLoggingApi, offlineIndicatorApi,
                feedKnownContent, tooltipApi, tooltipSupportedApi, applicationInfo,
                feedExtensionRegistry)
                                          .build();
        assertThat(streamScope.getStream()).isNotNull();
        assertThat(streamScope.getModelProviderFactory()).isNotNull();
    }

    @Test
    public void testComplexBuild() {
        StreamScope streamScope = new StreamScopeBuilder(context, actionApi, imageLoaderApi,
                protocolAdapter, feedSessionManager, threadUtils, timingUtils, taskQueue,
                mainThreadRunner, clock, DEBUG_BEHAVIOR, streamConfiguration, cardConfiguration,
                actionManager, config, snackbarApi, basicLoggingApi, offlineIndicatorApi,
                feedKnownContent, tooltipApi, tooltipSupportedApi, applicationInfo,
                feedExtensionRegistry)
                                          .setModelProviderFactory(modelProviderFactory)
                                          .setCustomElementProvider(customElementProvider)
                                          .setHostBindingProvider(new HostBindingProvider())
                                          .build();
        assertThat(streamScope.getStream()).isNotNull();
        assertThat(streamScope.getModelProviderFactory()).isEqualTo(modelProviderFactory);
    }

    @Test
    public void testStreamFactoryBuild() {
        setupStreamFactory(stream);

        StreamScope streamScope = new StreamScopeBuilder(context, actionApi, imageLoaderApi,
                protocolAdapter, feedSessionManager, threadUtils, timingUtils, taskQueue,
                mainThreadRunner, clock, DEBUG_BEHAVIOR, streamConfiguration, cardConfiguration,
                actionManager, config, snackbarApi, basicLoggingApi, offlineIndicatorApi,
                feedKnownContent, tooltipApi, tooltipSupportedApi, applicationInfo,
                feedExtensionRegistry)
                                          .setStreamFactory(streamFactory)
                                          .setCustomElementProvider(customElementProvider)
                                          .setHostBindingProvider(hostBindingProvider)
                                          .build();
        assertThat(streamScope.getStream()).isEqualTo(stream);
    }

    private void setupStreamFactory(Stream streamToReturn) {
        streamFactory = (actionParserFactory, context, buildType, cardConfiguration, imageLoaderApi,
                customElementProvider, debugBehavior, clock, modelProviderFactory,
                hostBindingProvider, offlineIndicatorApi, configuration, actionApi, actionManager,
                snackbarApi, streamConfiguration, feedExtensionRegistry, basicLoggingApi,
                mainThreadRunner, isBackgroundDark, tooltipApi, threadUtils,
                knownContentApi) -> streamToReturn;
    }
}
