// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

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

    private final Uri mUri;
    private final byte[] mBody;
    private final @HttpMethod String mMethod;
    private final List<HttpHeader> mHeaders;

    public HttpRequest(Uri uri, @HttpMethod String method, List<HttpHeader> headers, byte[] body) {
        this.mUri = uri;
        this.mBody = body;
        this.mMethod = method;
        this.mHeaders = Collections.unmodifiableList(new ArrayList<>(headers));
    }

    public Uri getUri() {
        return mUri;
    }

    public byte[] getBody() {
        return mBody;
    }

    public @HttpMethod String getMethod() {
        return mMethod;
    }

    public List<HttpHeader> getHeaders() {
        return mHeaders;
    }
}
