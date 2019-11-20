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

  private final NetworkClient directNetworkClient;
  private final ThreadUtils threadUtils;
  private final MainThreadRunner mainThreadRunner;

  public NetworkClientWrapper(
      NetworkClient directNetworkClient,
      ThreadUtils threadUtils,
      MainThreadRunner mainThreadRunner) {
    this.directNetworkClient = directNetworkClient;
    this.threadUtils = threadUtils;
    this.mainThreadRunner = mainThreadRunner;
  }

  @Override
  public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
    if (threadUtils.isMainThread()) {
      directNetworkClient.send(request, responseConsumer);
      return;
    }
    mainThreadRunner.execute(
        TAG + " send", () -> directNetworkClient.send(request, responseConsumer));
  }

  @Override
  public void close() throws Exception {
    directNetworkClient.close();
  }
}
