// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link Validators} class. */
@RunWith(RobolectricTestRunner.class)
public class ValidatorsTest {
    @Test
    public void testCheckNotNull_null() {
        assertThatRunnable(() -> Validators.checkNotNull(null))
                .throwsAnExceptionOfType(NullPointerException.class);
    }

    @Test
    public void testCheckNotNull_debugString() {
        String debugString = "Debug-String";
        assertThatRunnable(() -> Validators.checkNotNull(null, debugString))
                .throwsAnExceptionOfType(NullPointerException.class)
                .that()
                .hasMessageThat()
                .contains(debugString);
    }

    @Test
    public void testCheckNotNull_notNull() {
        String test = "test";
        assertThat(Validators.checkNotNull(test)).isEqualTo(test);
    }

    @Test
    public void testCheckState_trueWithMessagel() {
        // no exception expected
        Validators.checkState(true, "format-string");
    }

    @Test
    public void testCheckState_True() {
        // no exception expected
        Validators.checkState(true);
    }

    @Test
    public void testCheckState_falseWithMessage() {
        assertThatRunnable(() -> Validators.checkState(false, "format-string"))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testCheckState_false() {
        assertThatRunnable(() -> Validators.checkState(false))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }
}
