// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.common.intern;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link InternerWithStats} class. */
@RunWith(RobolectricTestRunner.class)
public class InternerWithStatsTest {

  private final InternerWithStats<StreamStructure> interner =
      new InternerWithStats<>(new WeakPoolInterner<>());

  @Before
  public void setUp() throws Exception {}

  @Test
  public void testBasic() {
    StreamStructure first =
        StreamStructure.newBuilder()
            .setContentId("foo")
            .setParentContentId("bar")
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();
    StreamStructure second =
        StreamStructure.newBuilder()
            .setContentId("foo")
            .setParentContentId("bar")
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();
    StreamStructure third =
        StreamStructure.newBuilder()
            .setContentId("foo")
            .setParentContentId("baz")
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();
    assertThat(first).isNotSameInstanceAs(second);
    assertThat(first).isEqualTo(second);
    assertThat(first).isNotEqualTo(third);

    // Pool is empty so first is added/returned.
    StreamStructure internedFirst = interner.intern(first);
    assertThat(internedFirst).isSameInstanceAs(first);
    assertThat(interner.getStats()).isEqualTo("Cache Hit Ratio: 0/1 (0.00)");

    // Pool already has an identical proto, which is returned.
    StreamStructure internedSecond = interner.intern(second);
    assertThat(internedSecond).isSameInstanceAs(first);
    assertThat(interner.getStats()).isEqualTo("Cache Hit Ratio: 1/2 (0.50)");

    // Third is a new object (not equal with any previous) so it is added to the pool.
    StreamStructure internedThird = interner.intern(third);
    assertThat(internedThird).isSameInstanceAs(third);
    assertThat(interner.getStats()).isEqualTo("Cache Hit Ratio: 1/3 (0.33)");

    interner.clear();
    assertThat(interner.size()).isEqualTo(0);
    assertThat(interner.getStats()).isEqualTo("Cache Hit Ratio: 0/0 (1.00)");
  }
}
