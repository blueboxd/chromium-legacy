// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.network.HttpHeader.HttpHeaderName;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Test class for {@link HttpRequest} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HttpRequestTest {
    public static final String HELLO_WORLD = "hello world";

    @Test
    public void testConstructor() throws Exception {
        Uri uri = Uri.EMPTY;
        String httpMethod = HttpMethod.GET;
        HttpRequest httpRequest =
                new HttpRequest(uri, httpMethod, Collections.emptyList(), new byte[] {});

        assertThat(httpRequest.getUri()).isEqualTo(uri);
        assertThat(httpRequest.getMethod()).isEqualTo(httpMethod);
    }

    @Test
    public void testConstructor_withHeaders() throws Exception {
        Uri uri = Uri.EMPTY;
        String httpMethod = HttpMethod.POST;
        List<HttpHeader> headers = Collections.singletonList(
                new HttpHeader(HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD, HELLO_WORLD));
        HttpRequest httpRequest = new HttpRequest(uri, httpMethod, headers, new byte[] {});

        assertThat(httpRequest.getUri()).isEqualTo(uri);
        assertThat(httpRequest.getMethod()).isEqualTo(httpMethod);
        assertThat(httpRequest.getHeaders()).isEqualTo(headers);
    }

    @Test
    public void testConstructor_withBody() throws Exception {
        Uri uri = Uri.EMPTY;
        byte[] bytes = new byte[] {};
        String httpMethod = HttpMethod.POST;
        HttpRequest httpRequest = new HttpRequest(uri, httpMethod, Collections.emptyList(), bytes);

        assertThat(httpRequest.getUri()).isEqualTo(uri);
        assertThat(httpRequest.getBody()).isEqualTo(bytes);
        assertThat(httpRequest.getMethod()).isEqualTo(httpMethod);
    }

    @Test
    public void testConstructor_withHeadersAndBody() throws Exception {
        Uri uri = Uri.EMPTY;
        byte[] bytes = new byte[] {};
        String httpMethod = HttpMethod.POST;
        List<HttpHeader> headers = Collections.singletonList(
                new HttpHeader(HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD, HELLO_WORLD));
        HttpRequest httpRequest = new HttpRequest(uri, httpMethod, headers, bytes);

        assertThat(httpRequest.getUri()).isEqualTo(uri);
        assertThat(httpRequest.getBody()).isEqualTo(bytes);
        assertThat(httpRequest.getMethod()).isEqualTo(httpMethod);
        assertThat(httpRequest.getHeaders()).isEqualTo(headers);
    }

    @Test
    public void immutableHeaders() {
        Uri uri = Uri.EMPTY;
        byte[] bytes = new byte[] {};
        String httpMethod = HttpMethod.POST;
        HttpHeader header =
                new HttpHeader(HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD, HELLO_WORLD);
        List<HttpHeader> headers = Collections.singletonList(header);
        HttpRequest httpRequest = new HttpRequest(uri, httpMethod, headers, bytes);

        List<HttpHeader> requestHeaders = httpRequest.getHeaders();
        assertThatRunnable(() -> requestHeaders.add(new HttpHeader("test", "testValue")))
                .throwsAnExceptionOfType(UnsupportedOperationException.class);
        assertThatRunnable(() -> requestHeaders.remove(header))
                .throwsAnExceptionOfType(UnsupportedOperationException.class);
    }
}
