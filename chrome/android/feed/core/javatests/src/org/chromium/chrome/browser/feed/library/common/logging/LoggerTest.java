// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.logging;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link Logger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LoggerTest {
    private static final Object[] EMPTY_ARRAY = {};

    @Test
    public void testBuildMessage_emptyString() {
        assertThat(Logger.buildMessage("", EMPTY_ARRAY)).isEmpty();
    }

    @Test
    public void testBuildMessage_nullString() {
        assertEquals("null", Logger.buildMessage(null, EMPTY_ARRAY));
    }

    @Test
    public void testBuildMessage_nullStringWithArgs() {
        // Test that any args are ignored
        assertEquals("null", Logger.buildMessage(null, new Object[] {"A", "B", 2}));
    }

    @Test
    public void testBuildMessage_noArgs() {
        assertEquals("hello", Logger.buildMessage("hello", EMPTY_ARRAY));
    }

    @Test
    public void testBuildMessage_formatString() {
        String format = "%s scolded %s %d times? %b/%b";
        assertEquals("A scolded B 2 times? true/false",
                Logger.buildMessage(format, new Object[] {"A", "B", 2, true, false}));
    }

    @Test
    public void testBuildMessage_formatStringForceArgsToString() {
        String format = "%s scolded %s %s times? %s/%s";
        assertEquals("A scolded B 2 times? true/false",
                Logger.buildMessage(format, new Object[] {"A", "B", 2, true, false}));
    }

    @Test
    public void testBuildMessage_formatStringNullArgs() {
        String format = "%s scolded %s %d times? %b/%b";
        assertEquals("null scolded null null times? false/false",
                Logger.buildMessage(format, new Object[] {null, null, null, null, null}));
    }

    @Test
    public void testBuildMessage_noFormat() {
        String format = "Hello";
        // Test that args are safely ignored
        assertEquals("Hello", Logger.buildMessage(format, new Object[] {"A", "B", 2}));
    }

    @Test
    public void testBuildMessage_illegalFormat() {
        String format = "Hello %d";
        // Test that args are concatenated when using a string for %d.
        assertEquals("Hello %d[A]", Logger.buildMessage(format, new Object[] {"A"}));
    }

    @Test
    public void testBuildMessage_arrayArgs() {
        String format = "%s %s";
        int[] a = {1, 2, 3};
        Object[] b = {"a", new String[] {"b", "c"}};
        assertEquals("[1, 2, 3] [a, [b, c]]", Logger.buildMessage(format, new Object[] {a, b}));
    }

    @Test
    public void testBuildMessage_arrayArgsWithNull() {
        String format = "%s";
        Object a = new String[] {"a", null};
        assertEquals("[a, null]", Logger.buildMessage(format, new Object[] {a}));
    }

    @Test
    public void testBuildMessage_arrayArgsWithCycle() {
        String format = "%s %s";
        Object[] a = new Object[1];
        Object[] b = {a};
        a[0] = b;
        assertEquals("[[[...]]] [[[...]]]", Logger.buildMessage(format, new Object[] {a, b}));
    }
}
