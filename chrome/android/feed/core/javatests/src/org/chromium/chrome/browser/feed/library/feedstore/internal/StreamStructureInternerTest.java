// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.intern.WeakPoolInterner;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link StreamStructureInterner} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamStructureInternerTest {
    private final StreamStructureInterner mInterner =
            new StreamStructureInterner(new WeakPoolInterner<>());

    @Test
    public void intern() {
        StreamStructure first = StreamStructure.newBuilder()
                                        .setContentId(newString("foo"))
                                        .setParentContentId(newString("bar"))
                                        .setOperation(Operation.UPDATE_OR_APPEND)
                                        .build();
        StreamStructure second = StreamStructure.newBuilder()
                                         .setContentId(newString("foo"))
                                         .setParentContentId(newString("baz"))
                                         .setOperation(Operation.UPDATE_OR_APPEND)
                                         .build();
        StreamStructure third = StreamStructure.newBuilder()
                                        .setContentId(newString("bar"))
                                        .setParentContentId(newString("foo"))
                                        .setOperation(Operation.UPDATE_OR_APPEND)
                                        .build();

        // Sanity check for the newString correct working.
        assertThat(first.getContentId()).isNotSameInstanceAs(second.getContentId());
        assertThat(first.getContentId()).isEqualTo(second.getContentId());

        // Pool is empty so first is added/returned.
        StreamStructure internedFirst = mInterner.intern(first);
        assertThat(mInterner.size()).isEqualTo(2); // {foo, bar}.
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has the "foo" content ID, which is reused.
        StreamStructure internedSecond = mInterner.intern(second);
        assertThat(internedSecond).isNotSameInstanceAs(second);
        assertThat(internedSecond).isEqualTo(second);
        // Content ID is the same as the one from first.
        assertThat(mInterner.size()).isEqualTo(3); // {foo, bar, baz}.
        assertThat(internedSecond.getContentId()).isSameInstanceAs(internedFirst.getContentId());

        // Pool already has both "foo" and "bar" content IDs, which are reused.
        StreamStructure internedThird = mInterner.intern(third);
        assertThat(internedThird).isNotSameInstanceAs(third);
        assertThat(internedThird).isEqualTo(third);
        // Content IDs are both reused.
        assertThat(mInterner.size()).isEqualTo(3); // {foo, bar, baz}.
        assertThat(internedThird.getContentId())
                .isSameInstanceAs(internedFirst.getParentContentId());
        assertThat(internedThird.getParentContentId())
                .isSameInstanceAs(internedFirst.getContentId());
    }

    // "new String()" below is called on purpose to generate different String objects.
    private String newString(String input) {
        return new String(input);
    }
}
