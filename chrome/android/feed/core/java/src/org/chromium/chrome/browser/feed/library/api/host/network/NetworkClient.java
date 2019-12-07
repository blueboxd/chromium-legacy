// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.network;

import org.chromium.base.Consumer;

/**
 * An object that can send an {@link HttpRequest} and receive an {@link HttpResponse} in response.
 */
public interface NetworkClient extends AutoCloseable {
    /**
     * Sends the HttpRequest. Upon completion, asynchronously calls the consumer with the
     * HttpResponse.
     *
     * <p>Requests and responses should be uncompressed when in the Feed. It is up to the host to
     * gzip / ungzip any requests or responses.
     *
     * @param request The request to send
     * @param responseConsumer The callback to be used when the response comes back.
     */
    void send(HttpRequest request, Consumer<HttpResponse> responseConsumer);
}
