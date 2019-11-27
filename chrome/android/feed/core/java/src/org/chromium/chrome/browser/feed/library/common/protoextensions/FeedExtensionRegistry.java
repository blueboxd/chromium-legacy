// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.protoextensions;

import com.google.protobuf.ExtensionRegistryLite;
import com.google.protobuf.GeneratedMessageLite.GeneratedExtension;

import org.chromium.chrome.browser.feed.library.api.host.proto.ProtoExtensionProvider;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.ClientBasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.PietExtensionsProto.PietFeedActionPayload;
import org.chromium.components.feed.core.proto.ui.stream.StreamOfflineExtensionProto.OfflineExtension;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.PietContent;
import org.chromium.components.feed.core.proto.ui.stream.StreamSwipeExtensionProto.SwipeActionExtension;
import org.chromium.components.feed.core.proto.wire.FeedActionRequestProto.FeedActionRequest;
import org.chromium.components.feed.core.proto.wire.FeedActionResponseProto.FeedActionResponse;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.FeedResponseProto.FeedResponse;
import org.chromium.components.feed.core.proto.wire.TokenProto;

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
