// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link HeaderViewHolder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HeaderViewHolderTest {
    private Context mContext;
    private HeaderViewHolder mHeaderViewHolder;
    private FrameLayout mFrameLayout;
    private FrameLayout mHeaderView;

    @Mock
    private Header mHeader;
    @Mock
    private SwipeNotifier mSwipeNotifier;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mFrameLayout = new FrameLayout(mContext);
        mHeaderView = new FrameLayout(mContext);
        mHeaderViewHolder = new HeaderViewHolder(mFrameLayout);
        when(mHeader.getView()).thenReturn(mHeaderView);
    }

    @Test
    public void testBind() {
        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);

        assertThat(mFrameLayout.getChildAt(0)).isSameInstanceAs(mHeaderView);
    }

    @Test
    public void testBind_removesHeaderFromPreviousView_ifHeaderViewAlreadyHasParent() {
        FrameLayout parentView = new FrameLayout(mContext);
        parentView.addView(mHeaderView);

        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);

        assertThat(parentView.getChildCount()).isEqualTo(0);
        assertThat(mFrameLayout.getChildAt(0)).isSameInstanceAs(mHeaderView);
    }

    @Test
    public void testUnbind() {
        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);
    }

    @Test
    public void testCanSwipe() {
        when(mHeader.isDismissible()).thenReturn(true);
        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);

        assertThat(mHeaderViewHolder.canSwipe()).isTrue();
    }

    @Test
    public void testCanSwipe_returnsFalse_whenViewHolderNotBound() {
        when(mHeader.isDismissible()).thenReturn(true);

        assertThat(mHeaderViewHolder.canSwipe()).isFalse();
    }

    @Test
    public void testOnSwiped() {
        mHeaderViewHolder.bind(mHeader, mSwipeNotifier);

        mHeaderViewHolder.onSwiped();

        verify(mSwipeNotifier).onSwiped();
    }

    @Test
    public void testOnSwiped_doesNotCallOnSwiped_whenViewHolderNotBound() {
        mHeaderViewHolder.onSwiped();

        verify(mSwipeNotifier, never()).onSwiped();
    }
}
