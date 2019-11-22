// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.scroll;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObserver;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link BasicStreamScrollMonitor}. */
@RunWith(RobolectricTestRunner.class)
public final class BasicStreamScrollMonitorTest {
    private static final String FEATURE_ID = "";
    private static final long TIMESTAMP = 150000L;

    @Mock
    private ScrollObserver scrollObserver1;
    @Mock
    private ScrollObserver scrollObserver2;

    private RecyclerView view;
    private FakeClock clock;
    private BasicStreamScrollMonitor scrollMonitor;

    @Before
    public void setUp() {
        initMocks(this);
        view = new RecyclerView(Robolectric.buildActivity(Activity.class).get());
        clock = new FakeClock();
        clock.set(TIMESTAMP);
        scrollMonitor = new BasicStreamScrollMonitor(clock);
    }

    @Test
    public void testCallbacks_onScroll() {
        scrollMonitor.onScrolled(view, 0, 0);

        scrollMonitor.addScrollObserver(scrollObserver1);

        scrollMonitor.onScrolled(view, 1, 1);
        scrollMonitor.onScrolled(view, 2, 2);

        scrollMonitor.addScrollObserver(scrollObserver2);

        scrollMonitor.onScrolled(view, 3, 3);
        scrollMonitor.onScrolled(view, 4, 4);

        scrollMonitor.removeScrollObserver(scrollObserver1);

        scrollMonitor.onScrolled(view, 5, 5);
        scrollMonitor.onScrolled(view, 6, 6);

        InOrder inOrder1 = inOrder(scrollObserver1);
        inOrder1.verify(scrollObserver1).onScroll(view, FEATURE_ID, 1, 1);
        inOrder1.verify(scrollObserver1).onScroll(view, FEATURE_ID, 2, 2);
        inOrder1.verify(scrollObserver1).onScroll(view, FEATURE_ID, 3, 3);
        inOrder1.verify(scrollObserver1).onScroll(view, FEATURE_ID, 4, 4);

        InOrder inOrder2 = inOrder(scrollObserver2);
        inOrder2.verify(scrollObserver2).onScroll(view, FEATURE_ID, 3, 3);
        inOrder2.verify(scrollObserver2).onScroll(view, FEATURE_ID, 4, 4);
        inOrder2.verify(scrollObserver2).onScroll(view, FEATURE_ID, 5, 5);
        inOrder2.verify(scrollObserver2).onScroll(view, FEATURE_ID, 6, 6);
    }

    @Test
    public void testCallbacks_onScrollStateChanged() {
        scrollMonitor.onScrollStateChanged(view, 0);

        scrollMonitor.addScrollObserver(scrollObserver1);

        scrollMonitor.onScrollStateChanged(view, 1);

        scrollMonitor.addScrollObserver(scrollObserver2);

        scrollMonitor.onScrollStateChanged(view, 2);

        scrollMonitor.removeScrollObserver(scrollObserver1);

        scrollMonitor.onScrollStateChanged(view, 1);
        scrollMonitor.onScrollStateChanged(view, 0);

        InOrder inOrder1 = inOrder(scrollObserver1);
        inOrder1.verify(scrollObserver1).onScrollStateChanged(view, FEATURE_ID, 1, TIMESTAMP);
        inOrder1.verify(scrollObserver1).onScrollStateChanged(view, FEATURE_ID, 2, TIMESTAMP);

        InOrder inOrder2 = inOrder(scrollObserver2);
        inOrder2.verify(scrollObserver2).onScrollStateChanged(view, FEATURE_ID, 2, TIMESTAMP);
        inOrder2.verify(scrollObserver2).onScrollStateChanged(view, FEATURE_ID, 1, TIMESTAMP);
        inOrder2.verify(scrollObserver2).onScrollStateChanged(view, FEATURE_ID, 0, TIMESTAMP);
    }

    @Test
    public void testCallbacks_afterEvent() {
        scrollMonitor.onScrolled(view, 1, 1);
        scrollMonitor.onScrolled(view, 2, 2);

        scrollMonitor.addScrollObserver(scrollObserver1);

        verify(scrollObserver1, never()).onScroll(any(View.class), anyString(), anyInt(), anyInt());
    }

    @Test
    public void testGetCurrentScrollState() {
        assertThat(scrollMonitor.getCurrentScrollState()).isEqualTo(RecyclerView.SCROLL_STATE_IDLE);
        scrollMonitor.onScrollStateChanged(view, RecyclerView.SCROLL_STATE_DRAGGING);
        assertThat(scrollMonitor.getCurrentScrollState())
                .isEqualTo(RecyclerView.SCROLL_STATE_DRAGGING);
    }
}
