// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.conformance.network;

import static com.google.common.truth.Truth.assertThat;

import android.net.Uri;

import org.junit.Test;

import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.common.testing.RequiredConsumer;

import java.util.Collections;

public abstract class NetworkClientConformanceTest {
    protected NetworkClient mNetworkClient;

    /** Defines a valid URI for the provided method. */
    protected Uri getValidUri(@HttpMethod String httpMethod) {
        return new Uri.Builder().path("http://www.google.com/").build();
    }

    /** Allows conformance tests to delay assertions until after the request completes. */
    protected void waitForRequest() {}

    @Test
    public void send_get() {
        @HttpMethod
        String method = HttpMethod.GET;
        HttpRequest request = new HttpRequest(
                getValidUri(method), method, Collections.emptyList(), new byte[] {});
        RequiredConsumer<HttpResponse> responseConsumer = new RequiredConsumer<>(response -> {
            assertThat(response.getResponseBody()).isNotNull();
            assertThat(response.getResponseCode()).isNotNull();
        });

        mNetworkClient.send(request, responseConsumer);
        waitForRequest();
        assertThat(responseConsumer.isCalled()).isTrue();
    }

    @Test
    public void send_post() {
        @HttpMethod
        String method = HttpMethod.POST;
        HttpRequest request = new HttpRequest(
                getValidUri(method), method, Collections.emptyList(), "helloWorld".getBytes());
        RequiredConsumer<HttpResponse> responseConsumer = new RequiredConsumer<>(response -> {
            assertThat(response.getResponseBody()).isNotNull();
            assertThat(response.getResponseCode()).isNotNull();
        });

        mNetworkClient.send(request, responseConsumer);
        waitForRequest();
        assertThat(responseConsumer.isCalled()).isTrue();
    }

    @Test
    public void send_put() {
        @HttpMethod
        String method = HttpMethod.PUT;
        HttpRequest request = new HttpRequest(
                getValidUri(method), method, Collections.emptyList(), "helloWorld".getBytes());
        RequiredConsumer<HttpResponse> responseConsumer = new RequiredConsumer<>(response -> {
            assertThat(response.getResponseBody()).isNotNull();
            assertThat(response.getResponseCode()).isNotNull();
        });

        mNetworkClient.send(request, responseConsumer);
        waitForRequest();
        assertThat(responseConsumer.isCalled()).isTrue();
    }

    @Test
    public void send_delete() {
        @HttpMethod
        String method = HttpMethod.DELETE;
        HttpRequest request = new HttpRequest(
                getValidUri(method), method, Collections.emptyList(), new byte[] {});
        RequiredConsumer<HttpResponse> responseConsumer = new RequiredConsumer<>(response -> {
            assertThat(response.getResponseBody()).isNotNull();
            assertThat(response.getResponseCode()).isNotNull();
        });

        mNetworkClient.send(request, responseConsumer);
        waitForRequest();
        assertThat(responseConsumer.isCalled()).isTrue();
    }
}
