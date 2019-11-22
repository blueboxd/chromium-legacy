// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.stream;

import android.content.Context;

import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo.BuildType;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;

/** Factory to allow creation of {@link Stream} instances that depend on common components. */
public interface StreamFactory {
    /** Create the {@link Stream} */
    Stream build(ActionParserFactory actionParserFactory, Context context, @BuildType int buildType,
            CardConfiguration cardConfiguration, ImageLoaderApi imageLoaderApi,
            CustomElementProvider customElementProvider, DebugBehavior debugBehavior, Clock clock,
            ModelProviderFactory modelProviderFactory, HostBindingProvider hostBindingProvider,
            OfflineIndicatorApi offlineIndicatorApi, Configuration configuration,
            ActionApi actionApi, ActionManager actionManager, SnackbarApi snackbarApi,
            StreamConfiguration streamConfiguration, FeedExtensionRegistry feedExtensionRegistry,
            BasicLoggingApi basicLoggingApi, MainThreadRunner mainThreadRunner,
            boolean isBackgroundDark, TooltipApi tooltipApi, ThreadUtils threadUtils,
            FeedKnownContent feedKnownContent);
}
