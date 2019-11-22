// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedapplifecyclelistener;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Test class for {@link FeedAppLifecycleListener} */
@RunWith(RobolectricTestRunner.class)
public class FeedAppLifecycleListenerTest {
    @Mock
    private FeedLifecycleListener lifecycleListener;
    @Mock
    private ThreadUtils threadUtils;
    private FeedAppLifecycleListener appLifecycleListener;

    @Before
    public void setup() {
        initMocks(this);
        appLifecycleListener = new FeedAppLifecycleListener(threadUtils);
        appLifecycleListener.registerObserver(lifecycleListener);
    }

    @Test
    public void onEnterForeground() {
        appLifecycleListener.onEnterForeground();
        verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
    }

    @Test
    public void onEnterBackground() {
        appLifecycleListener.onEnterBackground();
        verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
    }

    @Test
    public void onClearAll() {
        appLifecycleListener.onClearAll();
        verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL);
    }

    @Test
    public void onClearAllWithRefresh() {
        appLifecycleListener.onClearAllWithRefresh();
        verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL_WITH_REFRESH);
    }

    @Test
    public void onInitialize() {
        appLifecycleListener.initialize();
        verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.INITIALIZE);
    }
}
