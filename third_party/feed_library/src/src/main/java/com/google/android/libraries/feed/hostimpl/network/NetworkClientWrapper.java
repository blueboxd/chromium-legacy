// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.network;

import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;

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
