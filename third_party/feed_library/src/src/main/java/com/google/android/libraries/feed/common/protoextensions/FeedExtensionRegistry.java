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
    private final ExtensionRegistryLite extensionRegistry = ExtensionRegistryLite.newInstance();

    /**
     * Creates the registry.
     *
     * <p>TODO: Move this initialization code into Feed initialization, once that exists.
     */
    public FeedExtensionRegistry(ProtoExtensionProvider extensionProvider) {
        // Set up all the extensions we use inside the Feed.
        extensionRegistry.add(Card.cardExtension);
        extensionRegistry.add(ClientBasicLoggingMetadata.clientBasicLoggingMetadata);
        extensionRegistry.add(Content.contentExtension);
        extensionRegistry.add(FeedAction.feedActionExtension);
        extensionRegistry.add(FeedRequest.feedRequest);
        extensionRegistry.add(FeedResponse.feedResponse);
        extensionRegistry.add(FeedActionRequest.feedActionRequest);
        extensionRegistry.add(FeedActionResponse.feedActionResponse);
        extensionRegistry.add(OfflineExtension.offlineExtension);
        extensionRegistry.add(PietContent.pietContentExtension);
        extensionRegistry.add(PietFeedActionPayload.pietFeedActionPayloadExtension);
        extensionRegistry.add(StreamStructureProto.Stream.streamExtension);
        extensionRegistry.add(SwipeActionExtension.swipeActionExtension);
        extensionRegistry.add(TokenProto.Token.tokenExtension);

        // Call the host and add all the extensions it uses.
        for (GeneratedExtension<?, ?> extension : extensionProvider.getProtoExtensions()) {
            extensionRegistry.add(extension);
        }
    }

    /** Returns the {@link ExtensionRegistryLite}. */
    public ExtensionRegistryLite getExtensionRegistry() {
        return extensionRegistry.getUnmodifiable();
    }
}
