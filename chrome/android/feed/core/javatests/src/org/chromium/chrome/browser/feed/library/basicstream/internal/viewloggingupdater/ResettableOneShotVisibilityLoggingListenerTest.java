// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ResettableOneShotVisibilityLoggingListener}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ResettableOneShotVisibilityLoggingListenerTest {
    @Test
    public void testReset_logsTwice() {
        LoggingListener loggingListener = mock(LoggingListener.class);
        ResettableOneShotVisibilityLoggingListener resettableLoggingListener =
                new ResettableOneShotVisibilityLoggingListener(loggingListener);
        resettableLoggingListener.onViewVisible();
        resettableLoggingListener.reset();
        resettableLoggingListener.onViewVisible();
        resettableLoggingListener.onViewVisible();

        verify(loggingListener, times(2)).onViewVisible();
    }
}
