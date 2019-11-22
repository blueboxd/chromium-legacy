// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

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

import com.google.android.libraries.feed.piet.FrameAdapter;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.piet.testing.FakeFrameAdapter;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.common.collect.ImmutableList;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.PietProto.Frame;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PietScrollObserverTest}. */
@RunWith(RobolectricTestRunner.class)
public class PietScrollObserverTest {
    @Mock
    private ScrollObservable scrollObservable;
    @Mock
    private ActionHandler actionHandler;

    // .newBuilder().build() creates a unique instance we can check same() against.
    private static final Action VIEW_ACTION = Action.newBuilder().build();

    private FrameAdapter frameAdapter;
    private FrameLayout viewport;
    private LinearLayout frameView;
    private PietScrollObserver pietScrollObserver;

    @Before
    public void setUp() {
        initMocks(this);
        Activity activity = Robolectric.setupActivity(Activity.class);
        viewport = new FrameLayout(activity);
        activity.getWindow().addContentView(viewport, new LayoutParams(100, 100));

        frameAdapter = FakeFrameAdapter.builder(activity)
                               .setActionHandler(actionHandler)
                               .addViewAction(VIEW_ACTION)
                               .build();
        frameAdapter.bindModel(Frame.getDefaultInstance(), 0, null, ImmutableList.of());
        frameView = frameAdapter.getFrameContainer();

        pietScrollObserver = new PietScrollObserver(frameAdapter, viewport, scrollObservable);
    }

    @Test
    public void testTriggersActionsWhenIdle() {
        pietScrollObserver.onScrollStateChanged(viewport, "", RecyclerView.SCROLL_STATE_IDLE, 123L);

        verify(actionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(frameAdapter.getFrameContainer()),
                        any() /* LogData */);
    }

    @Test
    public void testDoesNotTriggerWhenScrolling() {
        pietScrollObserver.onScrollStateChanged(
                viewport, "", RecyclerView.SCROLL_STATE_DRAGGING, 123L);

        verifyZeroInteractions(actionHandler);
    }

    @Test
    public void testTriggersOnFirstDraw() {
        pietScrollObserver.installFirstDrawTrigger();
        when(scrollObservable.getCurrentScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);

        frameView.getViewTreeObserver().dispatchOnGlobalLayout();

        verify(actionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(frameView), any() /* LogData */);
    }

    @Test
    public void testDoesNotTriggerOnSecondDraw() {
        pietScrollObserver.installFirstDrawTrigger();
        when(scrollObservable.getCurrentScrollState())
                .thenReturn(RecyclerView.SCROLL_STATE_DRAGGING);

        // trigger on global layout - actions will not trigger because scrolling is not IDLE
        frameView.getViewTreeObserver().dispatchOnGlobalLayout();

        when(scrollObservable.getCurrentScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);

        // trigger on global layout - actions will not trigger because observer has been removed.
        frameView.getViewTreeObserver().dispatchOnGlobalLayout();

        verifyZeroInteractions(actionHandler);
    }

    @Test
    public void testTriggersOnAttach() {
        // Actions will not fire before attached to window
        frameView.getViewTreeObserver().dispatchOnGlobalLayout();
        verifyZeroInteractions(actionHandler);

        // Now attach to window and actions should fire
        viewport.addView(frameView);
        frameView.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(actionHandler)
                .handleAction(same(VIEW_ACTION), eq(ActionType.VIEW),
                        eq(Frame.getDefaultInstance()), eq(frameView), any() /* LogData */);
    }
}
