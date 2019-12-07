// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.requestmanager;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;

/**
 * Creates and issues requests to the server.
 *
 * <p>Note: this is the internal version of FeedRequestManager. See {@link RequestManager} for the
 * client version.
 */
public interface FeedRequestManager {
    /**
     * Issues a request for the next page of data. The {@code streamToken} described to the server
     * what the next page means. The{@code token} is used by the server for consistent data across
     * requests. The response will be sent to a {@link Consumer} with a {@link Model} created by the
     * ProtocolAdapter.
     */
    void loadMore(
            StreamToken streamToken, ConsistencyToken token, Consumer<Result<Model>> consumer);

    /**
     * Issues a request to refresh the entire feed, with the consumer being called back with the
     * resulting {@link Model}.
     *
     * @param reason The reason for this refresh.
     */
    void triggerRefresh(@RequestReason int reason, Consumer<Result<Model>> consumer);

    /**
     * Issues a request to refresh the entire feed, with the consumer being called back with the
     * resulting {@link Model}.
     *
     * @param reason The reason for this refresh.
     * @param token Used by the server for consistent data across requests.
     * @param consumer The consumer called after the refresh is performed.
     */
    void triggerRefresh(
            @RequestReason int reason, ConsistencyToken token, Consumer<Result<Model>> consumer);
}
