// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.network;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;

import java.util.ArrayList;

/** Fake implementation of {@link NetworkClient}. */
public final class FakeNetworkClient implements NetworkClient {
    private final FakeThreadUtils mFakeThreadUtils;
    private final ArrayList<HttpResponse> mResponses = new ArrayList<>();
    /*@Nullable*/ private HttpRequest mRequest;
    /*@Nullable*/ private HttpResponse mDefaultResponse;

    public FakeNetworkClient(FakeThreadUtils fakeThreadUtils) {
        this.mFakeThreadUtils = fakeThreadUtils;
    }

    @Override
    public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
        this.mRequest = request;
        boolean policy = mFakeThreadUtils.enforceMainThread(true);
        try {
            if (mResponses.isEmpty() && mDefaultResponse != null) {
                responseConsumer.accept(mDefaultResponse);
                return;
            }

            responseConsumer.accept(mResponses.remove(0));
        } finally {
            mFakeThreadUtils.enforceMainThread(policy);
        }
    }

    @Override
    public void close() {}

    /** Adds a response to the {@link FakeNetworkClient} to be returned in order. */
    public FakeNetworkClient addResponse(HttpResponse response) {
        mResponses.add(response);
        return this;
    }

    /** Sets a default response to use if no other response is available. */
    public FakeNetworkClient setDefaultResponse(HttpResponse response) {
        mDefaultResponse = response;
        return this;
    }

    /** Returns the last {@link HttpRequest} sent. */
    /*@Nullable*/
    public HttpRequest getLatestRequest() {
        return mRequest;
    }
}
