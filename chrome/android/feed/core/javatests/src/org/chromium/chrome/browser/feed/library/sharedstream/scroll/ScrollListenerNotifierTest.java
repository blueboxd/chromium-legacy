// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener.UNKNOWN_SCROLL_DELTA;
import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener.ScrollState;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ScrollListenerNotifier}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScrollListenerNotifierTest {
    private static final String FEATURE_ID = "feature";
    private static final long TIME = 12345L;

    @Mock
    private ScrollListener mScrollListener1;
    @Mock
    private ScrollListener mScrollListener2;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private ScrollObservable mScrollObservable;

    private ScrollListenerNotifier mScrollListenerNotifier;
    private RecyclerView mRecyclerView;
    private FakeMainThreadRunner mMainThreadRunner;
    private FakeClock mClock;

    @Before
    public void setUp() {
        initMocks(this);

        Context context = Robolectric.buildActivity(Activity.class).get();
        mRecyclerView = new RecyclerView(context);
        mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
        mClock = new FakeClock();
        mClock.set(TIME);

        mScrollListenerNotifier = new ScrollListenerNotifier(
                mContentChangedListener, mScrollObservable, mMainThreadRunner);
        mScrollListenerNotifier.addScrollListener(mScrollListener1);
    }

    @Test
    public void testConstructor() {
        ScrollListenerNotifier notifier = new ScrollListenerNotifier(
                mContentChangedListener, mScrollObservable, mMainThreadRunner);

        verify(mScrollObservable).addScrollObserver(notifier);
    }

    @Test
    public void testScrollStateOutputs() {
        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);
        verify(mScrollListener1).onScrollStateChanged(ScrollState.IDLE);

        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_DRAGGING, TIME);
        verify(mScrollListener1).onScrollStateChanged(ScrollState.DRAGGING);

        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_SETTLING, TIME);
        verify(mScrollListener1).onScrollStateChanged(ScrollState.SETTLING);

        assertThatRunnable(()
                                   -> mScrollListenerNotifier.onScrollStateChanged(
                                           mRecyclerView, FEATURE_ID, -42, TIME))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testOnScrollStateChanged() {
        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(mScrollListener1).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrollStateChanged_notifiesObserverOnIdle() {
        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(mContentChangedListener).onContentChanged();
    }

    @Test
    public void testOnScrollStateChanged_multipleListeners() {
        mScrollListenerNotifier.addScrollListener(mScrollListener2);

        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(mScrollListener1).onScrollStateChanged(ScrollState.IDLE);
        verify(mScrollListener2).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrollStateChanged_removedListener() {
        mScrollListenerNotifier.addScrollListener(mScrollListener2);
        mScrollListenerNotifier.removeScrollListener(mScrollListener1);

        mScrollListenerNotifier.onScrollStateChanged(
                mRecyclerView, FEATURE_ID, RecyclerView.SCROLL_STATE_IDLE, TIME);

        verify(mScrollListener1, never()).onScrollStateChanged(anyInt());
        verify(mScrollListener2).onScrollStateChanged(ScrollState.IDLE);
    }

    @Test
    public void testOnScrolled() {
        mScrollListenerNotifier.onScroll(mRecyclerView, FEATURE_ID, 1, 2);

        verify(mScrollListener1).onScrolled(1, 2);
    }

    @Test
    public void testOnScrolled_multipleListeners() {
        mScrollListenerNotifier.addScrollListener(mScrollListener2);
        mScrollListenerNotifier.onScroll(mRecyclerView, FEATURE_ID, 1, 2);

        verify(mScrollListener1).onScrolled(1, 2);
        verify(mScrollListener2).onScrolled(1, 2);
    }

    @Test
    public void testOnScrolled_removedListener() {
        mScrollListenerNotifier.addScrollListener(mScrollListener2);
        mScrollListenerNotifier.removeScrollListener(mScrollListener1);

        mScrollListenerNotifier.onScroll(mRecyclerView, FEATURE_ID, 1, 2);

        verify(mScrollListener1, never()).onScrolled(anyInt(), anyInt());
        verify(mScrollListener2).onScrolled(1, 2);
    }

    @Test
    public void onProgrammaticScroll() {
        mScrollListenerNotifier.onProgrammaticScroll(mRecyclerView);

        verify(mScrollListener1, never()).onScrolled(anyInt(), anyInt());

        mMainThreadRunner.runAllTasks();
        verify(mScrollListener1).onScrolled(UNKNOWN_SCROLL_DELTA, UNKNOWN_SCROLL_DELTA);
    }
}
