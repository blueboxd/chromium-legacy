// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * An object that can send an HTTP GET request and receive bytes in response. This interface should
 * only be used for fetching images.
 */
public interface ImageFetchClient {
    /**
     * HTTP response.
     */
    public interface HttpResponse {
        /**
         * HTTP status code if there was a response, or a net::Error if not.
         */
        default int status() {
            return -2; // net::FAILED
        }
        default byte[] body() {
            return new byte[0];
        }
    }

    /**
     * HTTP response callback interface.
     */
    public interface HttpResponseConsumer {
        default void requestComplete(HttpResponse response) {}
    }

    /**
     * Send a GET request. Upon completion, asynchronously calls the consumer with all body bytes
     * from the response.
     *
     * @param url URL to request
     * @param responseConsumer The callback to call with the response
     */
    default void sendRequest(String url, HttpResponseConsumer responseConsumer) {}
}
