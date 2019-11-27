// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import android.net.Uri;
import android.util.Base64;

import com.google.protobuf.CodedInputStream;

import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;

import java.io.IOException;
import java.util.Collections;

/** A class that helps build and sent requests to the server */
public final class RequestHelper {
    public static final String MOTHERSHIP_PARAM_PAYLOAD = "reqpld";
    public static final String LOCALE_PARAM = "hl";
    private static final String MOTHERSHIP_PARAM_FORMAT = "fmt";
    private static final String MOTHERSHIP_VALUE_BINARY = "bin";

    private RequestHelper() {}

    /**
     * Returns the first length-prefixed value from {@code input}. The first bytes of input are
     * assumed to be a varint32 encoding the length of the rest of the message. If input contains
     * more than one message, only the first message is returned.i w
     *
     * @throws IOException if input cannot be parsed
     */
    static byte[] getLengthPrefixedValue(byte[] input) throws IOException {
        CodedInputStream codedInputStream = CodedInputStream.newInstance(input);
        if (codedInputStream.isAtEnd()) {
            throw new IOException("Empty length-prefixed response");
        } else {
            int length = codedInputStream.readRawVarint32();
            return codedInputStream.readRawBytes(length);
        }
    }

    static HttpRequest buildHttpRequest(
            String httpMethod, byte[] bytes, String endpoint, String locale) {
        boolean isPostMethod = HttpMethod.POST.equals(httpMethod);
        Uri.Builder uriBuilder = Uri.parse(endpoint).buildUpon();
        if (!isPostMethod) {
            uriBuilder.appendQueryParameter(MOTHERSHIP_PARAM_PAYLOAD,
                    Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_WRAP));
        }

        uriBuilder.appendQueryParameter(MOTHERSHIP_PARAM_FORMAT, MOTHERSHIP_VALUE_BINARY);
        if (!locale.isEmpty()) {
            uriBuilder.appendQueryParameter(LOCALE_PARAM, locale);
        }

        return new HttpRequest(uriBuilder.build(), httpMethod, Collections.emptyList(),
                isPostMethod ? bytes : new byte[] {});
    }
}
