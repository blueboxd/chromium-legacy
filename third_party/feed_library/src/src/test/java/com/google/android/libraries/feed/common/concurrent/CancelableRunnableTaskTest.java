// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link TaskQueue} class. */
@RunWith(RobolectricTestRunner.class)
public class CancelableRunnableTaskTest {
    private boolean wasRun;
    private CancelableRunnableTask cancelable;

    @Before
    public void setUp() {
        cancelable = new CancelableRunnableTask(() -> { wasRun = true; });
    }

    @Test
    public void testRunnableCanceled_notCanceled() {
        cancelable.run();

        assertThat(wasRun).isTrue();
    }

    @Test
    public void testRunnableCanceled_canceled() {
        cancelable.cancel();
        cancelable.run();

        assertThat(wasRun).isFalse();
    }
}
