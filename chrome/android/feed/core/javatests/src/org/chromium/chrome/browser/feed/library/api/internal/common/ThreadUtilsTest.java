// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests of the {@link ThreadUtils} class. Robolectric runs everything on the main thread, so the
 * tests are written to test the checks properly.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ThreadUtilsTest {
    @Test
    public void testOnMainThread() {
        ThreadUtils threadUtils = new ThreadUtils();
        assertThat(threadUtils.isMainThread()).isTrue();
    }

    @Test
    public void testCheckMainThread() {
        ThreadUtils threadUtils = new ThreadUtils();
        // expect no exception
        threadUtils.checkMainThread();
    }

    @Test()
    public void testCheckNotMainThread() {
        final ThreadUtils threadUtils = new ThreadUtils();
        assertThatRunnable(threadUtils::checkNotMainThread)
                .throwsAnExceptionOfType(IllegalStateException.class);
    }
}
