// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.intern;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link WeakPoolInterner} class. */
@RunWith(RobolectricTestRunner.class)
public class WeakPoolInternerTest {
    private final WeakPoolInterner<StreamStructure> interner = new WeakPoolInterner<>();

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
        StreamStructure internedFirst = interner.intern(first);
        assertThat(interner.size()).isEqualTo(1);
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has an identical proto, which is returned.
        StreamStructure internedSecond = interner.intern(second);
        assertThat(interner.size()).isEqualTo(1);
        assertThat(internedSecond).isSameInstanceAs(first);

        // Third is a new object (not equal with any previous) so it is added to the pool.
        StreamStructure internedThird = interner.intern(third);
        assertThat(interner.size()).isEqualTo(2);
        assertThat(internedThird).isSameInstanceAs(third);

        interner.clear();
        assertThat(interner.size()).isEqualTo(0);

        // Pool is empty so second is added/returned.
        internedSecond = interner.intern(second);
        assertThat(interner.size()).isEqualTo(1);
        assertThat(internedSecond).isSameInstanceAs(second);
    }
}
