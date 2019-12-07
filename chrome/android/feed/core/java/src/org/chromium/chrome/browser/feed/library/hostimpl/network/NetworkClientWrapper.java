// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.network;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;

/** A {@link NetworkClient} which wraps a NetworkClient to make calls on the Main thread. */
public final class NetworkClientWrapper implements NetworkClient {
    private static final String TAG = "NetworkClientWrapper";

    private final NetworkClient mDirectNetworkClient;
    private final ThreadUtils mThreadUtils;
    private final MainThreadRunner mMainThreadRunner;

    public NetworkClientWrapper(NetworkClient directNetworkClient, ThreadUtils threadUtils,
            MainThreadRunner mainThreadRunner) {
        this.mDirectNetworkClient = directNetworkClient;
        this.mThreadUtils = threadUtils;
        this.mMainThreadRunner = mainThreadRunner;
    }

    @Override
    public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
        if (mThreadUtils.isMainThread()) {
            mDirectNetworkClient.send(request, responseConsumer);
            return;
        }
        mMainThreadRunner.execute(
                TAG + " send", () -> mDirectNetworkClient.send(request, responseConsumer));
    }

    @Override
    public void close() throws Exception {
        mDirectNetworkClient.close();
    }
}
