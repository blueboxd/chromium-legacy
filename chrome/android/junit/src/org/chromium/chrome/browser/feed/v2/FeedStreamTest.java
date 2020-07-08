// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.feed.shared.stream.Stream.POSITION_NOT_KNOWN;

import android.app.Activity;
import android.content.Context;
import android.util.TypedValue;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link FeedStream}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedStreamTest {
    private class FakeLinearLayoutManager extends LinearLayoutManager {
        private final List<View> mChildViews;
        private int mFirstVisiblePosition = RecyclerView.NO_POSITION;
        private int mLastVisiblePosition = RecyclerView.NO_POSITION;
        private int mItemCount = RecyclerView.NO_POSITION;

        public FakeLinearLayoutManager(Context context) {
            super(context);
            mChildViews = new ArrayList<>();
        }

        @Override
        public int findFirstVisibleItemPosition() {
            return mFirstVisiblePosition;
        }

        @Override
        public int findLastVisibleItemPosition() {
            return mLastVisiblePosition;
        }

        @Override
        public int getItemCount() {
            return mItemCount;
        }

        @Override
        public View findViewByPosition(int i) {
            if (i < 0 || i >= mChildViews.size()) {
                return null;
            }
            return mChildViews.get(i);
        }

        private void addChildToPosition(int position, View child) {
            mChildViews.add(position, child);
        }
    }

    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedStream mFeedStream;

    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private FeedStreamSurface.Natives mFeedStreamSurfaceJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedStreamSurfaceJni.TEST_HOOKS, mFeedStreamSurfaceJniMock);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);

        mFeedStream = new FeedStream(mActivity, false, mSnackbarManager, mPageNavigationDelegate,
                mBottomSheetController);
        mFeedStream.onCreate(null);
        mRecyclerView = (RecyclerView) mFeedStream.getView();
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);
    }

    @Test
    public void testIsChildAtPositionVisible() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mFeedStream.isChildAtPositionVisible(-2)).isFalse();
        assertThat(mFeedStream.isChildAtPositionVisible(-1)).isFalse();
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isTrue();
        assertThat(mFeedStream.isChildAtPositionVisible(1)).isTrue();
        assertThat(mFeedStream.isChildAtPositionVisible(2)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_nothingVisible() {
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validTop() {
        mLayoutManager.mFirstVisiblePosition = 0;
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validBottom() {
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testGetChildTopAt_noVisibleChild() {
        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt_noChild() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt() {
        mLayoutManager.mFirstVisiblePosition = 0;
        mLayoutManager.mLastVisiblePosition = 1;
        View view = new FrameLayout(mActivity);
        mLayoutManager.addChildToPosition(0, view);

        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(view.getTop());
    }

    @Test
    public void testCheckScrollingForLoadMore_StreamContentHidden() {
        // By default, stream content is not visible.
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    public void testCheckScrollingForLoadMore_StreamContentVisible() {
        mFeedStream.setStreamContentVisibility(true);
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;

        // loadMore not triggered due to not enough accumulated scrolling distance.
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.mLastVisiblePosition = itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1;
        mLayoutManager.mItemCount = itemCount;
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.mLastVisiblePosition = itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1;
        mLayoutManager.mItemCount = itemCount;
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    private int getLoadMoreTriggerScrollDistance() {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                FeedStream.LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP,
                mRecyclerView.getResources().getDisplayMetrics());
    }
}
