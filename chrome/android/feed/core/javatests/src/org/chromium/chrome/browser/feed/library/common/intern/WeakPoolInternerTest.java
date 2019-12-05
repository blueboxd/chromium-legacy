// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link WeakPoolInterner} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WeakPoolInternerTest {
    private final WeakPoolInterner<StreamStructure> mInterner = new WeakPoolInterner<>();

    @Test
    public void testBasic() {
        StreamStructure first = StreamStructure.newBuilder()
                                        .setContentId("foo")
                                        .setParentContentId("bar")
                                        .setOperation(Operation.UPDATE_OR_APPEND)
                                        .build();
        StreamStructure second = StreamStructure.newBuilder()
                                         .setContentId("foo")
                                         .setParentContentId("bar")
                                         .setOperation(Operation.UPDATE_OR_APPEND)
                                         .build();
        StreamStructure third = StreamStructure.newBuilder()
                                        .setContentId("foo")
                                        .setParentContentId("baz")
                                        .setOperation(Operation.UPDATE_OR_APPEND)
                                        .build();
        assertThat(first).isNotSameInstanceAs(second);
        assertThat(first).isEqualTo(second);
        assertThat(first).isNotEqualTo(third);

        // Pool is empty so first is added/returned.
        StreamStructure internedFirst = mInterner.intern(first);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has an identical proto, which is returned.
        StreamStructure internedSecond = mInterner.intern(second);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internedSecond).isSameInstanceAs(first);

        // Third is a new object (not equal with any previous) so it is added to the pool.
        StreamStructure internedThird = mInterner.intern(third);
        assertThat(mInterner.size()).isEqualTo(2);
        assertThat(internedThird).isSameInstanceAs(third);

        mInterner.clear();
        assertThat(mInterner.size()).isEqualTo(0);

        // Pool is empty so second is added/returned.
        internedSecond = mInterner.intern(second);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internedSecond).isSameInstanceAs(second);
    }
}
