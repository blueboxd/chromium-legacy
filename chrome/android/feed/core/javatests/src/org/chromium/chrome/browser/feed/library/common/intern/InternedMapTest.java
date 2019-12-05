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

import java.util.HashMap;

/** Tests of the {@link InternedMap} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InternedMapTest {
    private final InternedMap<String, StreamStructure> mMap =
            new InternedMap<>(new HashMap<>(), new WeakPoolInterner<>());

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

        // Initially the interner pool is empty so first is added as value.
        mMap.put("first", first);
        assertThat(mMap.get("first")).isSameInstanceAs(first);

        // The interner pool already has an identical value object so second is not added, first is
        // used instead.
        mMap.put("second", second);
        assertThat(mMap.get("second")).isSameInstanceAs(first);

        // Third is a new object (not equal with any previous) so it is added to the pool.
        mMap.put("third", third);
        assertThat(mMap.get("third")).isSameInstanceAs(third);

        // Since the pool is empty, seconds is added to the pool this time.
        mMap.clear();
        mMap.put("second", second);
        assertThat(mMap.get("second")).isSameInstanceAs(second);
    }
}
