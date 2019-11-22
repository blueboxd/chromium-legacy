// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ScrollType;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ScrollLogger}. */
@RunWith(RobolectricTestRunner.class)
public final class ScrollLoggerTest {
    @Mock
    private BasicLoggingApi basicLoggingApi;
    private ScrollLogger scrollLogger;

    @Before
    public void setUp() {
        initMocks(this);
        scrollLogger = new ScrollLogger(basicLoggingApi);
    }

    @Test
    public void testLogsScroll() {
        int scrollAmount = 100;
        scrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
        verify(basicLoggingApi).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }

    @Test
    public void testLogsNoScroll_tooSmall() {
        int scrollAmount = 1;
        scrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
        verify(basicLoggingApi, never()).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    }
}
