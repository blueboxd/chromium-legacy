// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import android.content.Context;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
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
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.scope.FeedStreamScope;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.stream.BasicStreamFactory;
import org.chromium.chrome.browser.feed.library.api.internal.stream.StreamFactory;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.feedactionparser.FeedActionParserFactory;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.FeedModelProviderFactory;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.piet.host.ThrowingCustomElementProvider;

/** A builder that creates a {@link StreamScope}. */
public final class StreamScopeBuilder {
    // Required external dependencies.
    private final Context mContext;
    private final ActionApi mActionApi;
    private final ImageLoaderApi mImageLoaderApi;

    private final ProtocolAdapter mProtocolAdapter;
    private final FeedSessionManager mFeedSessionManager;
    private final ThreadUtils mThreadUtils;
    private final TimingUtils mTimingUtils;
    private final TaskQueue mTaskQueue;
    private final MainThreadRunner mMainThreadRunner;
    private final Clock mClock;
    private final ActionManager mActionManager;
    private final CardConfiguration mCardConfiguration;
    private final StreamConfiguration mStreamConfiguration;
    private final DebugBehavior mDebugBehavior;
    private final Configuration mConfig;
    private final SnackbarApi mSnackbarApi;
    private final BasicLoggingApi mBasicLoggingApi;
    private final OfflineIndicatorApi mOfflineIndicatorApi;
    private final FeedKnownContent mFeedKnownContent;
    private final TooltipApi mTooltipApi;
    private final ApplicationInfo mApplicationInfo;
    private final FeedExtensionRegistry mFeedExtensionRegistry;
    private boolean mIsBackgroundDark;

    // Optional internal components to override the default implementations.
    /*@MonotonicNonNull*/ private ActionParserFactory mActionParserFactory;
    /*@MonotonicNonNull*/ private ModelProviderFactory mModelProviderFactory;
    /*@MonotonicNonNull*/ private Stream mStream;
    /*@MonotonicNonNull*/ private StreamFactory mStreamFactory;
    /*@MonotonicNonNull*/ private CustomElementProvider mCustomElementProvider;
    /*@MonotonicNonNull*/ private HostBindingProvider mHostBindingProvider;

    /** Construct this builder using {@link ProcessScope#createStreamScopeBuilder} */
    public StreamScopeBuilder(Context context, ActionApi actionApi, ImageLoaderApi imageLoaderApi,
            ProtocolAdapter protocolAdapter, FeedSessionManager feedSessionManager,
            ThreadUtils threadUtils, TimingUtils timingUtils, TaskQueue taskQueue,
            MainThreadRunner mainThreadRunner, Clock clock, DebugBehavior debugBehavior,
            StreamConfiguration streamConfiguration, CardConfiguration cardConfiguration,
            ActionManager actionManager, Configuration config, SnackbarApi snackbarApi,
            BasicLoggingApi basicLoggingApi, OfflineIndicatorApi offlineIndicatorApi,
            FeedKnownContent feedKnownContent, TooltipApi tooltipApi,
            TooltipSupportedApi tooltipSupportedApi, ApplicationInfo applicationInfo,
            FeedExtensionRegistry feedExtensionRegistry) {
        this.mContext = context;
        this.mActionApi = actionApi;
        this.mImageLoaderApi = imageLoaderApi;
        this.mProtocolAdapter = protocolAdapter;
        this.mFeedSessionManager = feedSessionManager;
        this.mThreadUtils = threadUtils;
        this.mTimingUtils = timingUtils;
        this.mTaskQueue = taskQueue;
        this.mMainThreadRunner = mainThreadRunner;
        this.mStreamConfiguration = streamConfiguration;
        this.mCardConfiguration = cardConfiguration;
        this.mClock = clock;
        this.mDebugBehavior = debugBehavior;
        this.mActionManager = actionManager;
        this.mConfig = config;
        this.mSnackbarApi = snackbarApi;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mOfflineIndicatorApi = offlineIndicatorApi;
        this.mFeedKnownContent = feedKnownContent;
        this.mTooltipApi = tooltipApi;
        this.mApplicationInfo = applicationInfo;
        this.mFeedExtensionRegistry = feedExtensionRegistry;
    }

    public StreamScopeBuilder setIsBackgroundDark(boolean isBackgroundDark) {
        this.mIsBackgroundDark = isBackgroundDark;
        return this;
    }

    public StreamScopeBuilder setStreamFactory(StreamFactory streamFactory) {
        this.mStreamFactory = streamFactory;
        return this;
    }

    public StreamScopeBuilder setModelProviderFactory(ModelProviderFactory modelProviderFactory) {
        this.mModelProviderFactory = modelProviderFactory;
        return this;
    }

    public StreamScopeBuilder setCustomElementProvider(
            CustomElementProvider customElementProvider) {
        this.mCustomElementProvider = customElementProvider;
        return this;
    }

    public StreamScopeBuilder setHostBindingProvider(HostBindingProvider hostBindingProvider) {
        this.mHostBindingProvider = hostBindingProvider;
        return this;
    }

    public StreamScope build() {
        if (mModelProviderFactory == null) {
            mModelProviderFactory = new FeedModelProviderFactory(mFeedSessionManager, mThreadUtils,
                    mTimingUtils, mTaskQueue, mMainThreadRunner, mConfig, mBasicLoggingApi);
        }
        if (mActionParserFactory == null) {
            mActionParserFactory = new FeedActionParserFactory(mProtocolAdapter, mBasicLoggingApi);
        }
        if (mCustomElementProvider == null) {
            mCustomElementProvider = new ThrowingCustomElementProvider();
        }
        if (mHostBindingProvider == null) {
            mHostBindingProvider = new HostBindingProvider();
        }
        if (mStreamFactory == null) {
            mStreamFactory = new BasicStreamFactory();
        }
        mStream = mStreamFactory.build(Validators.checkNotNull(mActionParserFactory), mContext,
                mApplicationInfo.getBuildType(), mCardConfiguration, mImageLoaderApi,
                Validators.checkNotNull(mCustomElementProvider), mDebugBehavior, mClock,
                Validators.checkNotNull(mModelProviderFactory),
                Validators.checkNotNull(mHostBindingProvider), mOfflineIndicatorApi, mConfig,
                mActionApi, mActionManager, mSnackbarApi, mStreamConfiguration,
                mFeedExtensionRegistry, mBasicLoggingApi, mMainThreadRunner, mIsBackgroundDark,
                mTooltipApi, mThreadUtils, mFeedKnownContent);
        return new FeedStreamScope(
                Validators.checkNotNull(mStream), Validators.checkNotNull(mModelProviderFactory));
    }
}
