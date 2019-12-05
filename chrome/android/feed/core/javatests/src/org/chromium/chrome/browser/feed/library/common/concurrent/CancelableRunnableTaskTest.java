// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link TaskQueue} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CancelableRunnableTaskTest {
    private boolean mWasRun;
    private CancelableRunnableTask mCancelable;

    @Before
    public void setUp() {
        mCancelable = new CancelableRunnableTask(() -> { mWasRun = true; });
    }

    @Test
    public void testRunnableCanceled_notCanceled() {
        mCancelable.run();

        assertThat(mWasRun).isTrue();
    }

    @Test
    public void testRunnableCanceled_canceled() {
        mCancelable.cancel();
        mCancelable.run();

        assertThat(mWasRun).isFalse();
    }
}
