// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Test class for {@link Result} */
@RunWith(RobolectricTestRunner.class)
public class ResultTest {
    private static final String HELLO_WORLD = "Hello World";

    @Test
    public void success() throws Exception {
        Result<String> result = Result.success(HELLO_WORLD);
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEqualTo(HELLO_WORLD);
    }

    @Test
    public void success_nullValue() throws Exception {
        Result<String> result = Result.success(null);
        assertThat(result.isSuccessful()).isTrue();
        assertThatRunnable(result::getValue).throwsAnExceptionOfType(NullPointerException.class);
    }

    @Test
    public void error() throws Exception {
        Result<String> result = Result.failure();
        assertThat(result.isSuccessful()).isFalse();
        assertThatRunnable(result::getValue).throwsAnExceptionOfType(IllegalStateException.class);
    }
}
