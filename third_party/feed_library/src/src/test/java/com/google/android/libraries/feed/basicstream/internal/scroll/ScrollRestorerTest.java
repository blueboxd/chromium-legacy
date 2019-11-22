// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.scroll;

import static com.google.android.libraries.feed.basicstream.internal.scroll.ScrollRestorer.nonRestoringRestorer;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollListenerNotifier;
import com.google.android.libraries.feed.testing.android.LinearLayoutManagerForTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ScrollRestorer}. */
@RunWith(RobolectricTestRunner.class)
public class ScrollRestorerTest {
    private static final int HEADER_COUNT = 3;
    private static final int TOP_POSITION = 4;
    private static final int BOTTOM_POSITION = 10;
    private static final int ABANDON_RESTORE_THRESHOLD = BOTTOM_POSITION;
    private static final int OFFSET = -10;
    private static final ScrollState SCROLL_STATE =
            ScrollState.newBuilder().setPosition(TOP_POSITION).setOffset(OFFSET).build();

    @Mock
    private ScrollListenerNotifier scrollListenerNotifier;
    @Mock
    private Configuration configuration;

    private RecyclerView recyclerView;
    private LinearLayoutManagerForTest layoutManager;
    private Context context;

    @Before
    public void setUp() {
        initMocks(this);

        when(configuration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(false);
        when(configuration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD), anyLong()))
                .thenReturn((long) ABANDON_RESTORE_THRESHOLD);

        context = Robolectric.buildActivity(Activity.class).get();

        recyclerView = new RecyclerView(context);
        layoutManager = new LinearLayoutManagerForTest(context);
        recyclerView.setLayoutManager(layoutManager);
    }

    @Test
    public void testRestoreScroll() {
        ScrollRestorer scrollRestorer = new ScrollRestorer(
                configuration, recyclerView, scrollListenerNotifier, SCROLL_STATE);
        scrollRestorer.maybeRestoreScroll();

        verify(scrollListenerNotifier).onProgrammaticScroll(recyclerView);
        assertThat(layoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(layoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testRestoreScroll_repeatedCall() {
        ScrollRestorer scrollRestorer = new ScrollRestorer(
                configuration, recyclerView, scrollListenerNotifier, createScrollState(10, 20));
        scrollRestorer.maybeRestoreScroll();

        layoutManager.scrollToPositionWithOffset(TOP_POSITION, OFFSET);
        scrollRestorer.maybeRestoreScroll();

        verify(scrollListenerNotifier).onProgrammaticScroll(recyclerView);
        assertThat(layoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(layoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testAbandonRestoringScroll() {
        layoutManager.scrollToPositionWithOffset(TOP_POSITION, OFFSET);

        ScrollRestorer scrollRestorer = new ScrollRestorer(
                configuration, recyclerView, scrollListenerNotifier, createScrollState(10, 20));
        scrollRestorer.abandonRestoringScroll();
        scrollRestorer.maybeRestoreScroll();

        assertThat(layoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(layoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testGetScrollStateForScrollPosition() {
        layoutManager.firstVisibleItemPosition = TOP_POSITION;
        layoutManager.firstVisibleViewOffset = OFFSET;
        View view = new View(context);
        view.setTop(OFFSET);
        layoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(configuration, recyclerView, scrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetBundleForScrollPosition_invalidPosition() {
        assertThat(nonRestoringRestorer(configuration, recyclerView, scrollListenerNotifier)
                           .getScrollStateForScrollRestore(10))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_properRestore() {
        when(configuration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        layoutManager.firstVisibleItemPosition = TOP_POSITION;
        layoutManager.firstVisibleViewOffset = OFFSET;
        layoutManager.lastVisibleItemPosition = BOTTOM_POSITION;
        View view = new View(context);
        view.setTop(OFFSET);
        layoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(configuration, recyclerView, scrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_badBottomPosition() {
        when(configuration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        layoutManager.firstVisibleItemPosition = TOP_POSITION;
        layoutManager.firstVisibleViewOffset = OFFSET;
        View view = new View(context);
        view.setTop(OFFSET);
        layoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(configuration, recyclerView, scrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isNull();
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_restorePastThreshold() {
        when(configuration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        layoutManager.firstVisibleItemPosition = TOP_POSITION;
        layoutManager.firstVisibleViewOffset = OFFSET;
        layoutManager.lastVisibleItemPosition = ABANDON_RESTORE_THRESHOLD + HEADER_COUNT + 1;
        View view = new View(context);
        view.setTop(OFFSET);
        layoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(configuration, recyclerView, scrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isNull();
    }

    private ScrollState createScrollState(int position, int offset) {
        return ScrollState.newBuilder().setPosition(position).setOffset(offset).build();
    }
}
