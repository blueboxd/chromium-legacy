// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link OneShotVisibilityLoggingListener}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OneShotVisibilityLoggingListenerTest {
    @Mock
    private LoggingListener mLoggingListener;

    private OneShotVisibilityLoggingListener mOneShotVisibilityLoggingListener;

    @Before
    public void setUp() {
        initMocks(this);
        mOneShotVisibilityLoggingListener = new OneShotVisibilityLoggingListener(mLoggingListener);
    }

    @Test
    public void testOnViewVisible() {
        mOneShotVisibilityLoggingListener.onViewVisible();

        verify(mLoggingListener).onViewVisible();
    }

    @Test
    public void testOnViewVisible_onlyNotifiesListenerOnce() {
        mOneShotVisibilityLoggingListener.onViewVisible();
        reset(mLoggingListener);

        mOneShotVisibilityLoggingListener.onViewVisible();

        verify(mLoggingListener, never()).onViewVisible();
    }

    @Test
    public void testOnContentClicked() {
        mOneShotVisibilityLoggingListener.onContentClicked();

        verify(mLoggingListener).onContentClicked();
    }
}
