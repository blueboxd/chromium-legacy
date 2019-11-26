// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedactionparser;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.feedactionparser.internal.PietFeedActionPayloadRetriever;

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
    public ActionParser build(Supplier</*@Nullable*/ ContentMetadata> contentMetadataSupplier) {
        return new FeedActionParser(mProtocolAdapter, mPietFeedActionPayloadRetriever,
                contentMetadataSupplier, mBasicLoggingApi);
    }
}
