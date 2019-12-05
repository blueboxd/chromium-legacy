// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheets;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link HashPoolInterner} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HashPoolInternerTest {
    private final HashPoolInterner<PietSharedStateWrapper> mInterner = new HashPoolInterner<>();

    @Before
    public void setUp() throws Exception {}

    @Test
    public void testBasic() {
        PietSharedStateWrapper first = new PietSharedStateWrapper(
                PietSharedState.newBuilder()
                        .addTemplates(Template.newBuilder().setTemplateId("foo").setStylesheets(
                                Stylesheets.newBuilder().addStylesheetIds("bar")))
                        .addStylesheets(Stylesheet.newBuilder().setStylesheetId("baz"))
                        .build());
        PietSharedStateWrapper second = new PietSharedStateWrapper(
                PietSharedState.newBuilder()
                        .addTemplates(Template.newBuilder().setTemplateId("foo").setStylesheets(
                                Stylesheets.newBuilder().addStylesheetIds("bar")))
                        .addStylesheets(Stylesheet.newBuilder().setStylesheetId("baz"))
                        .build());
        PietSharedStateWrapper third = new PietSharedStateWrapper(
                PietSharedState.newBuilder()
                        .addTemplates(Template.newBuilder().setTemplateId("foo").setStylesheets(
                                Stylesheets.newBuilder().addStylesheetIds("bar")))
                        .addStylesheets(Stylesheet.newBuilder().setStylesheetId("bay"))
                        .build());
        assertThat(first).isNotSameInstanceAs(second);
        assertThat(first).isNotSameInstanceAs(third);
        assertThat(second).isNotSameInstanceAs(third);

        // Pool is empty so first is added/returned.
        PietSharedStateWrapper internedFirst = mInterner.intern(first);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internedFirst).isSameInstanceAs(first);

        // Pool already has an identical proto, which is returned.
        PietSharedStateWrapper internSecond = mInterner.intern(second);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internSecond).isSameInstanceAs(first);

        // Third is a new object (not equal with any previous) so it is added to the pool.
        PietSharedStateWrapper internedThird = mInterner.intern(third);
        assertThat(mInterner.size()).isEqualTo(2);
        assertThat(internedThird).isSameInstanceAs(third);

        mInterner.clear();
        assertThat(mInterner.size()).isEqualTo(0);

        // Pool is empty so second is added/returned.
        internSecond = mInterner.intern(second);
        assertThat(mInterner.size()).isEqualTo(1);
        assertThat(internSecond).isSameInstanceAs(second);
    }

    private static class PietSharedStateWrapper {
        private final PietSharedState mPietSharedState;

        private PietSharedStateWrapper(PietSharedState pietSharedState) {
            this.mPietSharedState = pietSharedState;
        }

        @Override
        public int hashCode() {
            return mPietSharedState.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            // Equals should never be called with HashPoolInterner, it may be very expensive.
            fail();
            return super.equals(obj);
        }
    }
}
