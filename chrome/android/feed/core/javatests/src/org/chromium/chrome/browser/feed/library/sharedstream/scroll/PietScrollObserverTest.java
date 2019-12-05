// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.support.v7.widget.RecyclerView;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.FrameAdapter;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.piet.testing.FakeFrameAdapter;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PietScrollObserverTest}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietScrollObserverTest {
    @Mock
    private ScrollObservable mScrollObservable;
    @Mock
    private ActionHandler mActionHandler;

    // .newBuilder().build() creates a unique instance we can check same() against.
    private static final Action VIEW_ACTION = Action.newBuilder().build();

    private FrameAdapter mFrameAdapter;
    private FrameLayout mViewport;
    private LinearLayout mFrameView;
    private PietScrollObserver mPietScrollObserver;

    @Before
    public void setUp() {
        initMocks(this);
        Activity activity = Robolectric.setupActivity(Activity.class);
        mViewport = new FrameLayout(activity);
        activity.getWindow().addContentView(mViewport, new LayoutParams(100, 100));

        mFrameAdapter = FakeFrameAdapter.builder(activity)
                                .setActionHandler(mActionHandler)
                                .addViewAction(VIEW_ACTION)
                                .build();
        mFrameAdapter.bindModel(Frame.getDefaultInstance(), 0, null, ImmutableList.of());
        mFrameView = mFrameAdapter.getFrameContainer();

        mPietScrollObserver = new PietScrollObserver(mFrameAdapter, mViewport, mScrollObservable);
    }

    @Test
    public void testTriggersActionsWhenIdle() {
        mPietScrollObserver.onScrollStateChanged(
                mViewport, "", RecyclerView.SCROLL_STATE_IDLE, 123L);

        verify(mActionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(mFrameAdapter.getFrameContainer()),
                        any() /* LogData */);
    }

    @Test
    public void testDoesNotTriggerWhenScrolling() {
        mPietScrollObserver.onScrollStateChanged(
                mViewport, "", RecyclerView.SCROLL_STATE_DRAGGING, 123L);

        verifyZeroInteractions(mActionHandler);
    }

    @Test
    public void testTriggersOnFirstDraw() {
        mPietScrollObserver.installFirstDrawTrigger();
        when(mScrollObservable.getCurrentScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);

        mFrameView.getViewTreeObserver().dispatchOnGlobalLayout();

        verify(mActionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(mFrameView), any() /* LogData */);
    }

    @Test
    public void testDoesNotTriggerOnSecondDraw() {
        mPietScrollObserver.installFirstDrawTrigger();
        when(mScrollObservable.getCurrentScrollState())
                .thenReturn(RecyclerView.SCROLL_STATE_DRAGGING);

        // trigger on global layout - actions will not trigger because scrolling is not IDLE
        mFrameView.getViewTreeObserver().dispatchOnGlobalLayout();

        when(mScrollObservable.getCurrentScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);

        // trigger on global layout - actions will not trigger because observer has been removed.
        mFrameView.getViewTreeObserver().dispatchOnGlobalLayout();

        verifyZeroInteractions(mActionHandler);
    }

    @Test
    public void testTriggersOnAttach() {
        // Actions will not fire before attached to window
        mFrameView.getViewTreeObserver().dispatchOnGlobalLayout();
        verifyZeroInteractions(mActionHandler);

        // Now attach to window and actions should fire
        mViewport.addView(mFrameView);
        mFrameView.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mActionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(mFrameView), any() /* LogData */);
    }
}
