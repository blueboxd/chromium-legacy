// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.scroll;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.ScrollRestorer.nonRestoringRestorer;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.sharedstream.scroll.ScrollListenerNotifier;
import org.chromium.chrome.browser.feed.library.testing.android.LinearLayoutManagerForTest;
import org.chromium.components.feed.core.proto.libraries.sharedstream.ScrollStateProto.ScrollState;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ScrollRestorer}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScrollRestorerTest {
    private static final int HEADER_COUNT = 3;
    private static final int TOP_POSITION = 4;
    private static final int BOTTOM_POSITION = 10;
    private static final int ABANDON_RESTORE_THRESHOLD = BOTTOM_POSITION;
    private static final int OFFSET = -10;
    private static final ScrollState SCROLL_STATE =
            ScrollState.newBuilder().setPosition(TOP_POSITION).setOffset(OFFSET).build();

    @Mock
    private ScrollListenerNotifier mScrollListenerNotifier;
    @Mock
    private Configuration mConfiguration;

    private RecyclerView mRecyclerView;
    private LinearLayoutManagerForTest mLayoutManager;
    private Context mContext;

    @Before
    public void setUp() {
        initMocks(this);

        when(mConfiguration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(false);
        when(mConfiguration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD), anyLong()))
                .thenReturn((long) ABANDON_RESTORE_THRESHOLD);

        mContext = Robolectric.buildActivity(Activity.class).get();

        mRecyclerView = new RecyclerView(mContext);
        mLayoutManager = new LinearLayoutManagerForTest(mContext);
        mRecyclerView.setLayoutManager(mLayoutManager);
    }

    @Test
    public void testRestoreScroll() {
        ScrollRestorer scrollRestorer = new ScrollRestorer(
                mConfiguration, mRecyclerView, mScrollListenerNotifier, SCROLL_STATE);
        scrollRestorer.maybeRestoreScroll();

        verify(mScrollListenerNotifier).onProgrammaticScroll(mRecyclerView);
        assertThat(mLayoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(mLayoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testRestoreScroll_repeatedCall() {
        ScrollRestorer scrollRestorer = new ScrollRestorer(
                mConfiguration, mRecyclerView, mScrollListenerNotifier, createScrollState(10, 20));
        scrollRestorer.maybeRestoreScroll();

        mLayoutManager.scrollToPositionWithOffset(TOP_POSITION, OFFSET);
        scrollRestorer.maybeRestoreScroll();

        verify(mScrollListenerNotifier).onProgrammaticScroll(mRecyclerView);
        assertThat(mLayoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(mLayoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testAbandonRestoringScroll() {
        mLayoutManager.scrollToPositionWithOffset(TOP_POSITION, OFFSET);

        ScrollRestorer scrollRestorer = new ScrollRestorer(
                mConfiguration, mRecyclerView, mScrollListenerNotifier, createScrollState(10, 20));
        scrollRestorer.abandonRestoringScroll();
        scrollRestorer.maybeRestoreScroll();

        assertThat(mLayoutManager.scrolledPosition).isEqualTo(TOP_POSITION);
        assertThat(mLayoutManager.scrolledOffset).isEqualTo(OFFSET);
    }

    @Test
    public void testGetScrollStateForScrollPosition() {
        mLayoutManager.firstVisibleItemPosition = TOP_POSITION;
        mLayoutManager.firstVisibleViewOffset = OFFSET;
        View view = new View(mContext);
        view.setTop(OFFSET);
        mLayoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(mConfiguration, mRecyclerView, mScrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetBundleForScrollPosition_invalidPosition() {
        assertThat(nonRestoringRestorer(mConfiguration, mRecyclerView, mScrollListenerNotifier)
                           .getScrollStateForScrollRestore(10))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_properRestore() {
        when(mConfiguration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        mLayoutManager.firstVisibleItemPosition = TOP_POSITION;
        mLayoutManager.firstVisibleViewOffset = OFFSET;
        mLayoutManager.lastVisibleItemPosition = BOTTOM_POSITION;
        View view = new View(mContext);
        view.setTop(OFFSET);
        mLayoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(mConfiguration, mRecyclerView, mScrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_badBottomPosition() {
        when(mConfiguration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        mLayoutManager.firstVisibleItemPosition = TOP_POSITION;
        mLayoutManager.firstVisibleViewOffset = OFFSET;
        View view = new View(mContext);
        view.setTop(OFFSET);
        mLayoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(mConfiguration, mRecyclerView, mScrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isNull();
    }

    @Test
    public void testGetScrollStateForScrollPosition_abandonRestoreBelowFold_restorePastThreshold() {
        when(mConfiguration.getValueOrDefault(
                     eq(ConfigKey.ABANDON_RESTORE_BELOW_FOLD), anyBoolean()))
                .thenReturn(true);

        mLayoutManager.firstVisibleItemPosition = TOP_POSITION;
        mLayoutManager.firstVisibleViewOffset = OFFSET;
        mLayoutManager.lastVisibleItemPosition = ABANDON_RESTORE_THRESHOLD + HEADER_COUNT + 1;
        View view = new View(mContext);
        view.setTop(OFFSET);
        mLayoutManager.addChildToPosition(TOP_POSITION, view);
        ScrollState restorerScrollState =
                nonRestoringRestorer(mConfiguration, mRecyclerView, mScrollListenerNotifier)
                        .getScrollStateForScrollRestore(HEADER_COUNT);
        assertThat(restorerScrollState).isNull();
    }

    private ScrollState createScrollState(int position, int offset) {
        return ScrollState.newBuilder().setPosition(position).setOffset(offset).build();
    }
}
