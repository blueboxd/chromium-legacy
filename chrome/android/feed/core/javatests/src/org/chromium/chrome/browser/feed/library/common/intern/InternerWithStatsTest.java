// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link InternerWithStats} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InternerWithStatsTest {
    private final InternerWithStats<StreamStructure> mInterner =
            new InternerWithStats<>(new WeakPoolInterner<>());

    @Before
    public void setUp() throws Exception {}

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
        assertThat(internedFirst).isSameInstanceAs(first);
        assertThat(mInterner.getStats()).isEqualTo("Cache Hit Ratio: 0/1 (0.00)");

        // Pool already has an identical proto, which is returned.
        StreamStructure internedSecond = mInterner.intern(second);
        assertThat(internedSecond).isSameInstanceAs(first);
        assertThat(mInterner.getStats()).isEqualTo("Cache Hit Ratio: 1/2 (0.50)");

        // Third is a new object (not equal with any previous) so it is added to the pool.
        StreamStructure internedThird = mInterner.intern(third);
        assertThat(internedThird).isSameInstanceAs(third);
        assertThat(mInterner.getStats()).isEqualTo("Cache Hit Ratio: 1/3 (0.33)");

        mInterner.clear();
        assertThat(mInterner.size()).isEqualTo(0);
        assertThat(mInterner.getStats()).isEqualTo("Cache Hit Ratio: 0/0 (1.00)");
    }
}
