// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ScrollLogger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ScrollLoggerTest {
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    private ScrollLogger mScrollLogger;

    @Before
    public void setUp() {
        initMocks(this);
        mScrollLogger = new ScrollLogger(mBasicLoggingApi);
    }

    @Test
    public void testLogsScroll() {
        int scrollAmount = 100;
        mScrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
        verify(mBasicLoggingApi).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }

    @Test
    public void testLogsNoScroll_tooSmall() {
        int scrollAmount = 1;
        mScrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
        verify(mBasicLoggingApi, never()).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }
}
