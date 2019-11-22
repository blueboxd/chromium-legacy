// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link OneShotVisibilityLoggingListener}. */
@RunWith(RobolectricTestRunner.class)
public class OneShotVisibilityLoggingListenerTest {
    @Mock
    private LoggingListener loggingListener;

    private OneShotVisibilityLoggingListener oneShotVisibilityLoggingListener;

    @Before
    public void setUp() {
        initMocks(this);
        oneShotVisibilityLoggingListener = new OneShotVisibilityLoggingListener(loggingListener);
    }

    @Test
    public void testOnViewVisible() {
        oneShotVisibilityLoggingListener.onViewVisible();

        verify(loggingListener).onViewVisible();
    }

    @Test
    public void testOnViewVisible_onlyNotifiesListenerOnce() {
        oneShotVisibilityLoggingListener.onViewVisible();
        reset(loggingListener);

        oneShotVisibilityLoggingListener.onViewVisible();

        verify(loggingListener, never()).onViewVisible();
    }

    @Test
    public void testOnContentClicked() {
        oneShotVisibilityLoggingListener.onContentClicked();

        verify(loggingListener).onContentClicked();
    }
}
