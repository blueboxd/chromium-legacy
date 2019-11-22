// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.host.network;

import android.net.Uri;
import android.support.annotation.StringDef;

import java.net.HttpURLConnection;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Representation of an HTTP request. */
public final class HttpRequest {
    /**
     * These string values line up with HTTP method values, see {@link
     * HttpURLConnection#setRequestMethod(String)}.
     */
    @StringDef({
            HttpMethod.GET,
            HttpMethod.POST,
            HttpMethod.PUT,
            HttpMethod.DELETE,
    })
    public @interface HttpMethod {
        String GET = "GET";
        String POST = "POST";
        String PUT = "PUT";
        String DELETE = "DELETE";
    }

    private final Uri uri;
    private final byte[] body;
    private final @HttpMethod String method;
    private final List<HttpHeader> headers;

    public HttpRequest(Uri uri, @HttpMethod String method, List<HttpHeader> headers, byte[] body) {
        this.uri = uri;
        this.body = body;
        this.method = method;
        this.headers = Collections.unmodifiableList(new ArrayList<>(headers));
    }

    public Uri getUri() {
        return uri;
    }

    public byte[] getBody() {
        return body;
    }

    public @HttpMethod String getMethod() {
        return method;
    }

    public List<HttpHeader> getHeaders() {
        return headers;
    }
}
