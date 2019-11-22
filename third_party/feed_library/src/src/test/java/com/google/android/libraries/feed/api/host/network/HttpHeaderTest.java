// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.host.network;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.host.network.HttpHeader.HttpHeaderName;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Test class for {@link HttpHeader} */
@RunWith(RobolectricTestRunner.class)
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
