// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import android.content.Context;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;

/** Allows interaction with the Feed library at the process leve. */
public interface ProcessScope extends Dumpable {
    /** Returns the Feed library request manager. */
    RequestManager getRequestManager();

    /** Returns the Feed library task queue. */
    TaskQueue getTaskQueue();

    /** Returns the Feed library lifecycle listener. */
    AppLifecycleListener getAppLifecycleListener();

    /** Returns the Feed library known content. */
    KnownContent getKnownContent();

    /** Returns a {@link StreamScopeBuilder.Builder}. */
    StreamScopeBuilder createStreamScopeBuilder(Context context, ImageLoaderApi imageLoaderApi,
            ActionApi actionApi, StreamConfiguration streamConfiguration,
            CardConfiguration cardConfiguration, SnackbarApi snackbarApi,
            OfflineIndicatorApi offlineIndicatorApi, TooltipApi tooltipApi);

    /** Called to destroy the scope object. */
    void onDestroy();
}
