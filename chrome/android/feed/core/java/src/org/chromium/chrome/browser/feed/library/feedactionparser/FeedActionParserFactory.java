// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.feedactionparser.internal.PietFeedActionPayloadRetriever;

/** Default factory for the default {@link ActionParser} implementation. */
public final class FeedActionParserFactory implements ActionParserFactory {
    private final ProtocolAdapter mProtocolAdapter;
    private final PietFeedActionPayloadRetriever mPietFeedActionPayloadRetriever;
    private final BasicLoggingApi mBasicLoggingApi;

    public FeedActionParserFactory(
            ProtocolAdapter protocolAdapter, BasicLoggingApi basicLoggingApi) {
        this.mProtocolAdapter = protocolAdapter;
        this.mPietFeedActionPayloadRetriever = new PietFeedActionPayloadRetriever();
        this.mBasicLoggingApi = basicLoggingApi;
    }

    @Override
    public ActionParser build(Supplier<ContentMetadata> contentMetadataSupplier) {
        return new FeedActionParser(mProtocolAdapter, mPietFeedActionPayloadRetriever,
                contentMetadataSupplier, mBasicLoggingApi);
    }
}
