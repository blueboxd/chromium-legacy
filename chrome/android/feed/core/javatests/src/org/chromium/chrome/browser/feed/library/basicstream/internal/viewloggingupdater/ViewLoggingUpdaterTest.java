// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ViewLoggingUpdater}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewLoggingUpdaterTest {
    @Test
    public void testResetViewTracking_resetsListeners() {
        ResettableOneShotVisibilityLoggingListener listerner1 =
                mock(ResettableOneShotVisibilityLoggingListener.class);
        ResettableOneShotVisibilityLoggingListener listerner2 =
                mock(ResettableOneShotVisibilityLoggingListener.class);

        ViewLoggingUpdater viewLoggingUpdater = new ViewLoggingUpdater();

        viewLoggingUpdater.registerObserver(listerner1);
        viewLoggingUpdater.registerObserver(listerner2);

        viewLoggingUpdater.resetViewTracking();

        verify(listerner1).reset();
        verify(listerner2).reset();
    }
}
