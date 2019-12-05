// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.intern.WeakPoolInterner;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLegacyPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link StreamPayloadInterner} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamPayloadInternerTest {
    private final StreamPayloadInterner mInterner =
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
        StreamPayload internedFirst = mInterner.intern(first);
        assertThat(mInterner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has the "foo" content ID, which is reused.
        StreamPayload internedSecond = mInterner.intern(second);
        assertThat(internedSecond).isNotSameInstanceAs(second);
        assertThat(internedSecond).isEqualTo(second);
        // Content ID is the same as the one from first.
        assertThat(mInterner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedSecond.getStreamSharedState().getContentId())
                .isSameInstanceAs(internedFirst.getStreamFeature().getContentId());

        // Pool already has both "foo" and "bar" content IDs, which are reused.
        StreamPayload internedThird = mInterner.intern(third);
        assertThat(internedThird).isNotSameInstanceAs(third);
        assertThat(internedThird).isEqualTo(third);
        // Content IDs are both reused.
        assertThat(mInterner.size()).isEqualTo(2); // {foo, bar}.
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
