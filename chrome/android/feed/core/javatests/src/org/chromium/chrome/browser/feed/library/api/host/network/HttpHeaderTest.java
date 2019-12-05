// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.network.HttpHeader.HttpHeaderName;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test class for {@link HttpHeader} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HttpHeaderTest {
    private static final String HEADER_VALUE = "Hello world";

    @Test
    public void testConstructor() {
        HttpHeader header =
                new HttpHeader(HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD, HEADER_VALUE);

        assertThat(header.getName()).isEqualTo(HttpHeaderName.X_PROTOBUFFER_REQUEST_PAYLOAD);
        assertThat(header.getValue()).isEqualTo(HEADER_VALUE);
    }
}
