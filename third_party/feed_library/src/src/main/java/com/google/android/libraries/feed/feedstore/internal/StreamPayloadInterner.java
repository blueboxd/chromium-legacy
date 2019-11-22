// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.common.intern.Interner;
import com.google.android.libraries.feed.common.intern.ProtoStringInternerBase;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

/** Interner that interns content ID related strings from StreamPayload protos. */
public class StreamPayloadInterner extends ProtoStringInternerBase<StreamPayload> {
    StreamPayloadInterner(Interner<String> interner) {
        super(interner);
    }

    @Override
    public StreamPayload intern(StreamPayload input) {
        if (input.hasStreamFeature()) {
            StreamFeature.Builder builder = internStreamFeature(input.getStreamFeature());
            // If builder is not null we memoized something and  we need to replace the proto with
            // the modified proto.
            if (builder != null) {
                return input.toBuilder().setStreamFeature(builder).build();
            }
        } else if (input.hasStreamSharedState()) {
            StreamSharedState.Builder builder =
                    internStreamSharedState(input.getStreamSharedState());
            // If builder is not null we memoized something and  we need to replace the proto with
            // the modified proto.
            if (builder != null) {
                return input.toBuilder().setStreamSharedState(builder).build();
            }
        } else if (input.hasStreamToken()) {
            StreamToken.Builder builder = internStreamToken(input.getStreamToken());
            // If builder is not null we memoized something and  we need to replace the proto with
            // the modified proto.
            if (builder != null) {
                return input.toBuilder().setStreamToken(builder).build();
            }
        }

        // If we got here we did not memoized anything.
        return input;
    }

    private StreamFeature./*@Nullable*/ Builder internStreamFeature(StreamFeature input) {
        StreamFeature.Builder builder = null;
        builder = internSingleStringField(
                input, builder, StreamFeature::getContentId, StreamFeature.Builder::setContentId);
        builder = internSingleStringField(
                input, builder, StreamFeature::getParentId, StreamFeature.Builder::setParentId);
        return builder;
    }

    private StreamSharedState./*@Nullable*/ Builder internStreamSharedState(
            StreamSharedState input) {
        StreamSharedState.Builder builder = null;
        builder = internSingleStringField(input, builder, StreamSharedState::getContentId,
                StreamSharedState.Builder::setContentId);
        return builder;
    }

    private StreamToken./*@Nullable*/ Builder internStreamToken(StreamToken input) {
        StreamToken.Builder builder = null;
        builder = internSingleStringField(
                input, builder, StreamToken::getContentId, StreamToken.Builder::setContentId);
        builder = internSingleStringField(
                input, builder, StreamToken::getParentId, StreamToken.Builder::setParentId);
        return builder;
    }
}
