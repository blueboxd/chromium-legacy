// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.common.intern.WeakPoolInterner;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamLegacyPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link StreamPayloadInterner} class. */
@RunWith(RobolectricTestRunner.class)
public class StreamPayloadInternerTest {
    private final StreamPayloadInterner interner =
            new StreamPayloadInterner(new WeakPoolInterner<>());

    @Test
    public void intern() {
        StreamPayload first =
                StreamPayload.newBuilder()
                        .setStreamFeature(
                                StreamFeature.newBuilder()
                                        .setContentId(newString("foo"))
                                        .setParentId(newString("bar"))
                                        .setLegacyContent(
                                                StreamLegacyPayload.newBuilder().setType("type")))
                        .build();
        StreamPayload second =
                StreamPayload.newBuilder()
                        .setStreamSharedState(
                                StreamSharedState.newBuilder().setContentId(newString("foo")))
                        .build();
        StreamPayload third = StreamPayload.newBuilder()
                                      .setStreamToken(StreamToken.newBuilder()
                                                              .setContentId(newString("bar"))
                                                              .setParentId(newString("foo")))
                                      .build();

        // Sanity check for the newString correct working.
        assertThat(first.getStreamFeature().getContentId())
                .isNotSameInstanceAs(second.getStreamSharedState().getContentId());
        assertThat(first.getStreamFeature().getContentId())
                .isEqualTo(second.getStreamSharedState().getContentId());

        // Pool is empty so first is added/returned.
        StreamPayload internedFirst = interner.intern(first);
        assertThat(interner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has the "foo" content ID, which is reused.
        StreamPayload internedSecond = interner.intern(second);
        assertThat(internedSecond).isNotSameInstanceAs(second);
        assertThat(internedSecond).isEqualTo(second);
        // Content ID is the same as the one from first.
        assertThat(interner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedSecond.getStreamSharedState().getContentId())
                .isSameInstanceAs(internedFirst.getStreamFeature().getContentId());

        // Pool already has both "foo" and "bar" content IDs, which are reused.
        StreamPayload internedThird = interner.intern(third);
        assertThat(internedThird).isNotSameInstanceAs(third);
        assertThat(internedThird).isEqualTo(third);
        // Content IDs are both reused.
        assertThat(interner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedThird.getStreamToken().getContentId())
                .isSameInstanceAs(internedFirst.getStreamFeature().getParentId());
        assertThat(internedThird.getStreamToken().getParentId())
                .isSameInstanceAs(internedFirst.getStreamFeature().getContentId());
    }

    // "new String()" below is called on purpose to generate different String objects.
    private String newString(String input) {
        return new String(input);
    }
}
