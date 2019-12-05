// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.logging;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.logging.Dumper.DumperValue;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.StringWriter;

/** Tests of the {@link Dumper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DumperTest {
    private static final String KEY1 = "keyOne";
    private static final String KEY2 = "keyTwo";
    private static final String KEY3 = "keyThree";
    private static final String VALUE1 = "valueOne";
    private static final String VALUE2 = "valueTwo";

    @Test
    public void testBaseDumper() {
        Dumper dumper = Dumper.newDefaultDumper();
        dumper.forKey(KEY1).value(13);
        assertThat(dumper.mValues).hasSize(1);
        DumperValue dumperValue = dumper.mValues.get(0);
        assertThat(dumperValue).isNotNull();
        assertThat(dumperValue.mName).isEqualTo(KEY1);
        assertThat(dumperValue.mContent.toString()).isEqualTo(Integer.toString(13));
        assertThat(dumperValue.mIndentLevel).isEqualTo(1);
    }

    @Test
    public void testBaseDumper_childDumper() {
        Dumper dumper = Dumper.newDefaultDumper();
        dumper.forKey(KEY1).value(13);
        assertThat(dumper.mValues).hasSize(1);

        Dumper childDumper = dumper.getChildDumper();
        childDumper.forKey(KEY2).value(17);

        assertThat(childDumper.mValues).isEqualTo(dumper.mValues);
        assertThat(childDumper.mValues).hasSize(2);

        DumperValue dumperValue = dumper.mValues.get(1);
        assertThat(dumperValue).isNotNull();
        assertThat(dumperValue.mName).isEqualTo(KEY2);
        assertThat(dumperValue.mContent.toString()).isEqualTo(Integer.toString(17));
        assertThat(dumperValue.mIndentLevel).isEqualTo(2);
    }

    @Test
    public void testDumpable() {
        Dumper dumper = Dumper.newDefaultDumper();
        TestDumpable testDumpable = new TestDumpable(KEY1);
        dumper.dump(testDumpable);
        testDumpable.assertValue(dumper.mValues.get(0));
    }

    @Test
    public void testDumpable_nested() {
        Dumper dumper = Dumper.newDefaultDumper();
        TestDumpable dumpableChild = new TestDumpable(KEY1);
        TestDumpable dumpable = new TestDumpable(dumpableChild, KEY2);
        dumper.dump(dumpable);

        assertThat(dumper.mValues).hasSize(2);
        dumpable.assertValue(dumper.mValues.get(0));
        dumpableChild.assertValue(dumper.mValues.get(1));
    }

    @Test
    public void testDumpable_cycle() {
        Dumper dumper = Dumper.newDefaultDumper();
        TestDumpable dumpableChild = new TestDumpable(KEY1);
        TestDumpable dumpable = new TestDumpable(dumpableChild, KEY2);
        dumpableChild.setDumpable(dumpable);

        dumper.dump(dumpable);

        // Two dumpables + the a message that statesa cycle was detected
        assertThat(dumper.mValues).hasSize(3);
        dumpable.assertValue(dumper.mValues.get(0));
        dumpableChild.assertValue(dumper.mValues.get(1));
    }

    @Test
    public void testDumpable_threeChildCycle() {
        Dumper dumper = Dumper.newDefaultDumper();
        TestDumpable dumpableThree = new TestDumpable(KEY1);
        TestDumpable dumpableTwo = new TestDumpable(dumpableThree, KEY2);
        TestDumpable dumpableOne = new TestDumpable(dumpableTwo, KEY3);
        dumpableThree.setDumpable(dumpableOne);

        dumper.dump(dumpableOne);

        // 3 dumpables + the a message that states cycle was detected
        assertThat(dumper.mValues).hasSize(4);
    }

    @Test
    public void testWrite() throws Exception {
        Dumper dumper = Dumper.newDefaultDumper();
        TestDumpable testDumpable = new TestDumpable(KEY1);
        dumper.dump(testDumpable);
        StringWriter stringWriter = new StringWriter();
        dumper.write(stringWriter);
        String dump = stringWriter.toString();
        assertThat(dump).contains(KEY1);
        assertThat(dump).contains("13");
    }

    @Test
    public void testWrite_compact() throws Exception {
        Dumper dumper = Dumper.newDefaultDumper();
        dumper.forKey(KEY1).value(VALUE1);
        dumper.forKey(KEY2).value(VALUE2).compactPrevious();
        StringWriter stringWriter = new StringWriter();
        dumper.write(stringWriter);
        String dump = stringWriter.toString();
        assertThat(dump).contains(" | ");
        assertThat(dump).contains(VALUE1);
        assertThat(dump).contains(VALUE2);
    }

    @Test
    public void testRedacted() throws Exception {
        Dumper dumper = Dumper.newRedactedDumper();
        dumper.forKey(KEY1).value(VALUE1).sensitive();
        dumper.forKey(KEY2).value(VALUE2).compactPrevious();
        StringWriter stringWriter = new StringWriter();
        dumper.write(stringWriter);
        String dump = stringWriter.toString();
        assertThat(dump).contains(" | ");
        assertThat(dump).contains(DumperValue.REDACTED);
        assertThat(dump).doesNotContain(VALUE1);
        assertThat(dump).contains(VALUE2);
    }

    private static class TestDumpable implements Dumpable {
        /*@Nullable*/ private Dumpable mChild;
        private final String mKey;

        TestDumpable(String key) {
            this(null, key);
        }

        TestDumpable(/*@Nullable*/ Dumpable child, String key) {
            this.mChild = child;
            this.mKey = key;
        }

        void setDumpable(Dumpable child) {
            this.mChild = child;
        }

        @Override
        public void dump(Dumper dumper) {
            dumper.forKey(mKey).value(13);
            dumper.dump(mChild);
        }

        void assertValue(DumperValue value) {
            assertThat(value.mName).isEqualTo(mKey);
            assertThat(value.mContent.toString()).isEqualTo(Integer.toString(13));
        }
    }
}
