// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.scroll;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link BasicStreamScrollMonitor}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class BasicStreamScrollMonitorTest {
    private static final String FEATURE_ID = "";
    private static final long TIMESTAMP = 150000L;

    @Mock
    private ScrollObserver mScrollObserver1;
    @Mock
    private ScrollObserver mScrollObserver2;

    private RecyclerView mView;
    private FakeClock mClock;
    private BasicStreamScrollMonitor mScrollMonitor;

    @Before
    public void setUp() {
        initMocks(this);
        mView = new RecyclerView(Robolectric.buildActivity(Activity.class).get());
        mClock = new FakeClock();
        mClock.set(TIMESTAMP);
        mScrollMonitor = new BasicStreamScrollMonitor(mClock);
    }

    @Test
    public void testCallbacks_onScroll() {
        mScrollMonitor.onScrolled(mView, 0, 0);

        mScrollMonitor.addScrollObserver(mScrollObserver1);

        mScrollMonitor.onScrolled(mView, 1, 1);
        mScrollMonitor.onScrolled(mView, 2, 2);

        mScrollMonitor.addScrollObserver(mScrollObserver2);

        mScrollMonitor.onScrolled(mView, 3, 3);
        mScrollMonitor.onScrolled(mView, 4, 4);

        mScrollMonitor.removeScrollObserver(mScrollObserver1);

        mScrollMonitor.onScrolled(mView, 5, 5);
        mScrollMonitor.onScrolled(mView, 6, 6);

        InOrder inOrder1 = inOrder(mScrollObserver1);
        inOrder1.verify(mScrollObserver1).onScroll(mView, FEATURE_ID, 1, 1);
        inOrder1.verify(mScrollObserver1).onScroll(mView, FEATURE_ID, 2, 2);
        inOrder1.verify(mScrollObserver1).onScroll(mView, FEATURE_ID, 3, 3);
        inOrder1.verify(mScrollObserver1).onScroll(mView, FEATURE_ID, 4, 4);

        InOrder inOrder2 = inOrder(mScrollObserver2);
        inOrder2.verify(mScrollObserver2).onScroll(mView, FEATURE_ID, 3, 3);
        inOrder2.verify(mScrollObserver2).onScroll(mView, FEATURE_ID, 4, 4);
        inOrder2.verify(mScrollObserver2).onScroll(mView, FEATURE_ID, 5, 5);
        inOrder2.verify(mScrollObserver2).onScroll(mView, FEATURE_ID, 6, 6);
    }

    @Test
    public void testCallbacks_onScrollStateChanged() {
        mScrollMonitor.onScrollStateChanged(mView, 0);

        mScrollMonitor.addScrollObserver(mScrollObserver1);

        mScrollMonitor.onScrollStateChanged(mView, 1);

        mScrollMonitor.addScrollObserver(mScrollObserver2);

        mScrollMonitor.onScrollStateChanged(mView, 2);

        mScrollMonitor.removeScrollObserver(mScrollObserver1);

        mScrollMonitor.onScrollStateChanged(mView, 1);
        mScrollMonitor.onScrollStateChanged(mView, 0);

        InOrder inOrder1 = inOrder(mScrollObserver1);
        inOrder1.verify(mScrollObserver1).onScrollStateChanged(mView, FEATURE_ID, 1, TIMESTAMP);
        inOrder1.verify(mScrollObserver1).onScrollStateChanged(mView, FEATURE_ID, 2, TIMESTAMP);

        InOrder inOrder2 = inOrder(mScrollObserver2);
        inOrder2.verify(mScrollObserver2).onScrollStateChanged(mView, FEATURE_ID, 2, TIMESTAMP);
        inOrder2.verify(mScrollObserver2).onScrollStateChanged(mView, FEATURE_ID, 1, TIMESTAMP);
        inOrder2.verify(mScrollObserver2).onScrollStateChanged(mView, FEATURE_ID, 0, TIMESTAMP);
    }

    @Test
    public void testCallbacks_afterEvent() {
        mScrollMonitor.onScrolled(mView, 1, 1);
        mScrollMonitor.onScrolled(mView, 2, 2);

        mScrollMonitor.addScrollObserver(mScrollObserver1);

        verify(mScrollObserver1, never())
                .onScroll(any(View.class), anyString(), anyInt(), anyInt());
    }

    @Test
    public void testGetCurrentScrollState() {
        assertThat(mScrollMonitor.getCurrentScrollState())
                .isEqualTo(RecyclerView.SCROLL_STATE_IDLE);
        mScrollMonitor.onScrollStateChanged(mView, RecyclerView.SCROLL_STATE_DRAGGING);
        assertThat(mScrollMonitor.getCurrentScrollState())
                .isEqualTo(RecyclerView.SCROLL_STATE_DRAGGING);
    }
}
