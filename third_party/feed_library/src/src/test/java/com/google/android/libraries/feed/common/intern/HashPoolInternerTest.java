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
import static org.junit.Assert.fail;

import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Stylesheets;
import com.google.search.now.ui.piet.PietProto.Template;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link HashPoolInterner} class. */
@RunWith(RobolectricTestRunner.class)
public class HashPoolInternerTest {

  private final HashPoolInterner<PietSharedStateWrapper> interner = new HashPoolInterner<>();

  @Before
  public void setUp() throws Exception {}

  @Test
  public void testBasic() {
    PietSharedStateWrapper first =
        new PietSharedStateWrapper(
            PietSharedState.newBuilder()
                .addTemplates(
                    Template.newBuilder()
                        .setTemplateId("foo")
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("bar")))
                .addStylesheets(Stylesheet.newBuilder().setStylesheetId("baz"))
                .build());
    PietSharedStateWrapper second =
        new PietSharedStateWrapper(
            PietSharedState.newBuilder()
                .addTemplates(
                    Template.newBuilder()
                        .setTemplateId("foo")
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("bar")))
                .addStylesheets(Stylesheet.newBuilder().setStylesheetId("baz"))
                .build());
    PietSharedStateWrapper third =
        new PietSharedStateWrapper(
            PietSharedState.newBuilder()
                .addTemplates(
                    Template.newBuilder()
                        .setTemplateId("foo")
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("bar")))
                .addStylesheets(Stylesheet.newBuilder().setStylesheetId("bay"))
                .build());
    assertThat(first).isNotSameInstanceAs(second);
    assertThat(first).isNotSameInstanceAs(third);
    assertThat(second).isNotSameInstanceAs(third);

    // Pool is empty so first is added/returned.
    PietSharedStateWrapper internedFirst = interner.intern(first);
    assertThat(interner.size()).isEqualTo(1);
    assertThat(internedFirst).isSameInstanceAs(first);

    // Pool already has an identical proto, which is returned.
    PietSharedStateWrapper internSecond = interner.intern(second);
    assertThat(interner.size()).isEqualTo(1);
    assertThat(internSecond).isSameInstanceAs(first);

    // Third is a new object (not equal with any previous) so it is added to the pool.
    PietSharedStateWrapper internedThird = interner.intern(third);
    assertThat(interner.size()).isEqualTo(2);
    assertThat(internedThird).isSameInstanceAs(third);

    interner.clear();
    assertThat(interner.size()).isEqualTo(0);

    // Pool is empty so second is added/returned.
    internSecond = interner.intern(second);
    assertThat(interner.size()).isEqualTo(1);
    assertThat(internSecond).isSameInstanceAs(second);
  }

  private static class PietSharedStateWrapper {
    private final PietSharedState pietSharedState;

    private PietSharedStateWrapper(PietSharedState pietSharedState) {
      this.pietSharedState = pietSharedState;
    }

    @Override
    public int hashCode() {
      return pietSharedState.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
      // Equals should never be called with HashPoolInterner, it may be very expensive.
      fail();
      return super.equals(obj);
    }
  }
}
