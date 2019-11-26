// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.protoextensions;

import com.google.android.libraries.feed.api.host.proto.ProtoExtensionProvider;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.GeneratedMessageLite.GeneratedExtension;
import com.google.search.now.feed.client.StreamDataProto.ClientBasicLoggingMetadata;
import com.google.search.now.ui.action.FeedActionProto.FeedAction;
import com.google.search.now.ui.action.PietExtensionsProto.PietFeedActionPayload;
import com.google.search.now.ui.stream.StreamOfflineExtensionProto.OfflineExtension;
import com.google.search.now.ui.stream.StreamStructureProto;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.ui.stream.StreamSwipeExtensionProto.SwipeActionExtension;
import com.google.search.now.wire.feed.FeedActionRequestProto.FeedActionRequest;
import com.google.search.now.wire.feed.FeedActionResponseProto.FeedActionResponse;
import com.google.search.now.wire.feed.FeedRequestProto.FeedRequest;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponse;
import com.google.search.now.wire.feed.TokenProto;

/**
 * Creates and initializes the proto extension registry, adding feed-internal extensions as well as
 * those provided by the host through the {@link ProtoExtensionProvider}.
 */
public class FeedExtensionRegistry {
    private final ExtensionRegistryLite mExtensionRegistry = ExtensionRegistryLite.newInstance();

    /**
     * Creates the registry.
     *
     * <p>TODO: Move this initialization code into Feed initialization, once that exists.
     */
    public FeedExtensionRegistry(ProtoExtensionProvider extensionProvider) {
        // Set up all the extensions we use inside the Feed.
        mExtensionRegistry.add(Card.cardExtension);
        mExtensionRegistry.add(ClientBasicLoggingMetadata.clientBasicLoggingMetadata);
        mExtensionRegistry.add(Content.contentExtension);
        mExtensionRegistry.add(FeedAction.feedActionExtension);
        mExtensionRegistry.add(FeedRequest.feedRequest);
        mExtensionRegistry.add(FeedResponse.feedResponse);
        mExtensionRegistry.add(FeedActionRequest.feedActionRequest);
        mExtensionRegistry.add(FeedActionResponse.feedActionResponse);
        mExtensionRegistry.add(OfflineExtension.offlineExtension);
        mExtensionRegistry.add(PietContent.pietContentExtension);
        mExtensionRegistry.add(PietFeedActionPayload.pietFeedActionPayloadExtension);
        mExtensionRegistry.add(StreamStructureProto.Stream.streamExtension);
        mExtensionRegistry.add(SwipeActionExtension.swipeActionExtension);
        mExtensionRegistry.add(TokenProto.Token.tokenExtension);

        // Call the host and add all the extensions it uses.
        for (GeneratedExtension<?, ?> extension : extensionProvider.getProtoExtensions()) {
            mExtensionRegistry.add(extension);
        }
    }

    /** Returns the {@link ExtensionRegistryLite}. */
    public ExtensionRegistryLite getExtensionRegistry() {
        return mExtensionRegistry.getUnmodifiable();
    }
}
