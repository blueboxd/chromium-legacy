// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

import static com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener.UNKNOWN_SCROLL_DELTA;
import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener;
import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener.ScrollState;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ScrollListenerNotifier}. */
@RunWith(RobolectricTestRunner.class)
public class ScrollListenerNotifierTest {
    private static final String FEATURE_ID = "feature";
    private static final long TIME = 12345L;

    @Mock
    private ScrollListener scrollListener1;
    @Mock
    private ScrollListener scrollListener2;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private ScrollObservable scrollObservable;

    private ScrollListenerNotifier scrollListenerNotifier;
    private RecyclerView recyclerView;
    private FakeMainThreadRunner mainThreadRunner;
    private FakeClock clock;

    @Before
    public void setUp() {
        initMocks(this);

        Context context = Robolectric.buildActivity(Activity.class).get();
        recyclerView = new RecyclerView(context);
        mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
        clock = new FakeClock();
        clock.set(TIME);

        scrollListenerNotifier = new ScrollListenerNotifier(
                contentChangedListener, scrollObservable, mainThreadRunner);
        scrollListenerNotifier.addScrollListener(scrollListener1);
    }

    @Test
    public void testConstructor() {
        ScrollListenerNotifier notifier = new ScrollListenerNotifier(
                contentChangedListener, scrollObservable, mainThreadRunner);

        verify(scrollObservable).addScrollObserver(notifier);
    }

    @Test
    public void testScrollStateOutputs() {
        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);
        verify(scrollListener1).onScrollStateChanged(ScrollState.IDLE);

        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_DRAGGING, TIME);
        verify(scrollListener1).onScrollStateChanged(ScrollState.DRAGGING);

        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_SETTLING, TIME);
        verify(scrollListener1).onScrollStateChanged(ScrollState.SETTLING);

        assertThatRunnable(()
                                   -> scrollListenerNotifier.onScrollStateChanged(
                                           recyclerView, FEATURE_ID, -42, TIME))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testOnScrollStateChanged() {
        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(scrollListener1).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrollStateChanged_notifiesObserverOnIdle() {
        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testOnScrollStateChanged_multipleListeners() {
        scrollListenerNotifier.addScrollListener(scrollListener2);

        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(scrollListener1).onScrollStateChanged(ScrollState.IDLE);
        verify(scrollListener2).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrollStateChanged_removedListener() {
        scrollListenerNotifier.addScrollListener(scrollListener2);
        scrollListenerNotifier.removeScrollListener(scrollListener1);

        scrollListenerNotifier.onScrollStateChanged(
                recyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(scrollListener1, never()).onScrollStateChanged(anyInt());
        verify(scrollListener2).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrolled() {
        scrollListenerNotifier.onScroll(recyclerView, FEATURE_ID, 1, 2);

        verify(scrollListener1).onScrolled(1, 2);
    }

    @Test
    public void testOnScrolled_multipleListeners() {
        scrollListenerNotifier.addScrollListener(scrollListener2);
        scrollListenerNotifier.onScroll(recyclerView, FEATURE_ID, 1, 2);

        verify(scrollListener1).onScrolled(1, 2);
        verify(scrollListener2).onScrolled(1, 2);
    }

    @Test
    public void testOnScrolled_removedListener() {
        scrollListenerNotifier.addScrollListener(scrollListener2);
        scrollListenerNotifier.removeScrollListener(scrollListener1);

        scrollListenerNotifier.onScroll(recyclerView, FEATURE_ID, 1, 2);

        verify(scrollListener1, never()).onScrolled(anyInt(), anyInt());
        verify(scrollListener2).onScrolled(1, 2);
    }

    @Test
    public void onProgrammaticScroll() {
        scrollListenerNotifier.onProgrammaticScroll(recyclerView);

        verify(scrollListener1, never()).onScrolled(anyInt(), anyInt());

        mainThreadRunner.runAllTasks();
        verify(scrollListener1).onScrolled(UNKNOWN_SCROLL_DELTA, UNKNOWN_SCROLL_DELTA);
    }
}
