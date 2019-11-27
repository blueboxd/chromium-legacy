// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.stream;

import android.content.Context;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo.BuildType;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.basicstream.BasicStream;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;

import java.util.ArrayList;

/** Factory to create {@link BasicStream} classes */
public class BasicStreamFactory implements StreamFactory {
    @Override
    public Stream build(ActionParserFactory actionParserFactory, Context context,
            @BuildType int buildType, CardConfiguration cardConfiguration,
            ImageLoaderApi imageLoaderApi, CustomElementProvider customElementProvider,
            DebugBehavior debugBehavior, Clock clock, ModelProviderFactory modelProviderFactory,
            HostBindingProvider hostBindingProvider, OfflineIndicatorApi offlineIndicatorApi,
            Configuration configuration, ActionApi actionApi, ActionManager actionManager,
            SnackbarApi snackbarApi, StreamConfiguration streamConfiguration,
            FeedExtensionRegistry feedExtensionRegistry, BasicLoggingApi basicLoggingApi,
            MainThreadRunner mainThreadRunner, boolean isBackgroundDark, TooltipApi tooltipApi,
            ThreadUtils threadUtils, FeedKnownContent feedKnownContent) {
        return new BasicStream(context, streamConfiguration, cardConfiguration, imageLoaderApi,
                actionParserFactory, actionApi, customElementProvider, debugBehavior, threadUtils,
                /* headers = */ new ArrayList<>(0), clock, modelProviderFactory,
                hostBindingProvider, actionManager, configuration, snackbarApi, basicLoggingApi,
                offlineIndicatorApi, mainThreadRunner, feedKnownContent, tooltipApi,
                isBackgroundDark);
    }
}
