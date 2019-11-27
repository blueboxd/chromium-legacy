// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.testing;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.ROOT_CONTENT_ID;

import com.google.protobuf.ByteString;

import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.WireProtocolInfo;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;

import java.nio.charset.Charset;

/**
 * Creates a paging based responses. Create two {@link Response} object, and initial response and a
 * page response. In addition, create a {@link StreamToken} which is used to trigger the paged
 * response.
 */
public class PagingState {
    public final Response initialResponse;
    public final Response pageResponse;
    public final StreamToken streamToken;

    public PagingState(ContentId[] initialCards, ContentId[] pageCards, int tokenId,
            ContentIdGenerators idGenerator) {
        ByteString token = ByteString.copyFrom(Integer.toString(tokenId), Charset.defaultCharset());
        initialResponse = getInitialResponse(initialCards, tokenId, token);
        streamToken = getSessionId(token, tokenId, idGenerator);
        pageResponse = getPageResponse(pageCards);
    }

    private static Response getInitialResponse(ContentId[] cards, int tokenId, ByteString token) {
        ResponseBuilder responseBuilder =
                ResponseBuilder.forClearAllWithCards(cards).addStreamToken(tokenId, token);
        WireProtocolInfo info = responseBuilder.getWireProtocolInfo();
        assertThat(info.hasToken).isTrue();
        return responseBuilder.build();
    }

    private static Response getPageResponse(ContentId[] pagedCards) {
        return ResponseBuilder.builder().addCardsToRoot(pagedCards).build();
    }

    private static StreamToken getSessionId(
            ByteString token, int id, ContentIdGenerators idGenerator) {
        ContentId tokenId = ContentId.newBuilder()
                                    .setContentDomain("token")
                                    .setId(id)
                                    .setTable("feature")
                                    .build();

        StreamToken.Builder tokenBuilder = StreamToken.newBuilder();
        tokenBuilder.setParentId(idGenerator.createContentId(ROOT_CONTENT_ID));
        tokenBuilder.setContentId(idGenerator.createContentId(tokenId));
        tokenBuilder.setNextPageToken(token);
        return tokenBuilder.build();
    }
}
