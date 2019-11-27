// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

import android.support.annotation.StringDef;

public final class HttpHeader {
    /** These string values correspond with the actual header name. */
    @StringDef({
            HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD,
    })
    public @interface HttpHeaderName {
        String X_PROTOBUFFER_REQUEST_PAYLOAD = "X-Protobuffer-Request-Payload";
    }

    public HttpHeader(@HttpHeaderName String name, String value) {
        this.name = name;
        this.value = value;
    }

    public final @HttpHeaderName String name;
    public final String value;

    public @HttpHeaderName String getName() {
        return name;
    }

    public String getValue() {
        return value;
    }
}
