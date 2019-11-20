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

package com.google.android.libraries.feed.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.common.intern.WeakPoolInterner;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link StreamStructureInterner} class. */
@RunWith(RobolectricTestRunner.class)
public class StreamStructureInternerTest {

  private final StreamStructureInterner interner =
      new StreamStructureInterner(new WeakPoolInterner<>());

  @Test
  public void intern() {
    StreamStructure first =
        StreamStructure.newBuilder()
            .setContentId(newString("foo"))
            .setParentContentId(newString("bar"))
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();
    StreamStructure second =
        StreamStructure.newBuilder()
            .setContentId(newString("foo"))
            .setParentContentId(newString("baz"))
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();
    StreamStructure third =
        StreamStructure.newBuilder()
            .setContentId(newString("bar"))
            .setParentContentId(newString("foo"))
            .setOperation(Operation.UPDATE_OR_APPEND)
            .build();

    // Sanity check for the newString correct working.
    assertThat(first.getContentId()).isNotSameInstanceAs(second.getContentId());
    assertThat(first.getContentId()).isEqualTo(second.getContentId());

    // Pool is empty so first is added/returned.
    StreamStructure internedFirst = interner.intern(first);
    assertThat(interner.size()).isEqualTo(2); // {foo, bar}.
    assertThat(internedFirst).isSameInstanceAs(first);

    // Pool already has the "foo" content ID, which is reused.
    StreamStructure internedSecond = interner.intern(second);
    assertThat(internedSecond).isNotSameInstanceAs(second);
    assertThat(internedSecond).isEqualTo(second);
    // Content ID is the same as the one from first.
    assertThat(interner.size()).isEqualTo(3); // {foo, bar, baz}.
    assertThat(internedSecond.getContentId()).isSameInstanceAs(internedFirst.getContentId());

    // Pool already has both "foo" and "bar" content IDs, which are reused.
    StreamStructure internedThird = interner.intern(third);
    assertThat(internedThird).isNotSameInstanceAs(third);
    assertThat(internedThird).isEqualTo(third);
    // Content IDs are both reused.
    assertThat(interner.size()).isEqualTo(3); // {foo, bar, baz}.
    assertThat(internedThird.getContentId()).isSameInstanceAs(internedFirst.getParentContentId());
    assertThat(internedThird.getParentContentId()).isSameInstanceAs(internedFirst.getContentId());
  }

  // "new String()" below is called on purpose to generate different String objects.
  private String newString(String input) {
    return new String(input);
  }
}
