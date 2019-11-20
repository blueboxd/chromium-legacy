// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.testing.network;

import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.ArrayList;

/** Fake implementation of {@link NetworkClient}. */
public final class FakeNetworkClient implements NetworkClient {
  private final FakeThreadUtils fakeThreadUtils;
  private final ArrayList<HttpResponse> responses = new ArrayList<>();
  /*@Nullable*/ private HttpRequest request = null;
  /*@Nullable*/ private HttpResponse defaultResponse = null;

  public FakeNetworkClient(FakeThreadUtils fakeThreadUtils) {
    this.fakeThreadUtils = fakeThreadUtils;
  }

  @Override
  public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
    this.request = request;
    boolean policy = fakeThreadUtils.enforceMainThread(true);
    try {
      if (responses.isEmpty() && defaultResponse != null) {
        responseConsumer.accept(defaultResponse);
        return;
      }

      responseConsumer.accept(responses.remove(0));
    } finally {
      fakeThreadUtils.enforceMainThread(policy);
    }
  }

  @Override
  public void close() {}

  /** Adds a response to the {@link FakeNetworkClient} to be returned in order. */
  public FakeNetworkClient addResponse(HttpResponse response) {
    responses.add(response);
    return this;
  }

  /** Sets a default response to use if no other response is available. */
  public FakeNetworkClient setDefaultResponse(HttpResponse response) {
    defaultResponse = response;
    return this;
  }

  /** Returns the last {@link HttpRequest} sent. */
  /*@Nullable*/
  public HttpRequest getLatestRequest() {
    return request;
  }
}
