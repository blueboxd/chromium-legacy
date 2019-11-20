// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

  private final ProtocolAdapter protocolAdapter;
  private final PietFeedActionPayloadRetriever pietFeedActionPayloadRetriever;
  private final BasicLoggingApi basicLoggingApi;

  public FeedActionParserFactory(ProtocolAdapter protocolAdapter, BasicLoggingApi basicLoggingApi) {
    this.protocolAdapter = protocolAdapter;
    this.pietFeedActionPayloadRetriever = new PietFeedActionPayloadRetriever();
    this.basicLoggingApi = basicLoggingApi;
  }

  @Override
  public ActionParser build(Supplier</*@Nullable*/ ContentMetadata> contentMetadataSupplier) {
    return new FeedActionParser(
        protocolAdapter, pietFeedActionPayloadRetriever, contentMetadataSupplier, basicLoggingApi);
  }
}
