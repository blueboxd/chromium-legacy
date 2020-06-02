// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feed.shared.stream.Header;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.feed.R;

import java.util.ArrayList;
import java.util.List;

/**
 * A implementation of a Feed {@link Stream} that is just able to render a vertical stream of
 * cards for Feed v2.
 */
public class FeedStream implements Stream {
    private static final String TAG = "FeedStream";

    private final Context mContext;
    private final FeedStreamSurface mFeedStreamSurface;
    private final ObserverList<ScrollListener> mScrollListeners;

    private RecyclerView mRecyclerView;
    // TODO(jianli): To be used.
    private boolean mIsStreamContentVisible = true;

    public FeedStream(Context context, boolean isBackgroundDark) {
        this.mFeedStreamSurface = new FeedStreamSurface(null, () -> null, context);
        this.mContext =
                new ContextThemeWrapper(context, (isBackgroundDark ? R.style.Dark : R.style.Light));
        this.mScrollListeners = new ObserverList<ScrollListener>();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        // TODO(jianli): TO be removed from Stream API since it is never used.
    }

    @Override
    public void onCreate(@Nullable String savedInstanceState) {
        setupRecyclerView();
        // TODO(jianli): Restore scroll state.
    }

    @Override
    public void onShow() {}

    @Override
    public void onActive() {}

    @Override
    public void onInactive() {}

    @Override
    public void onHide() {}

    @Override
    public void onDestroy() {
        mFeedStreamSurface.destroy();
    }

    @Override
    public Bundle getSavedInstanceState() {
        // TODO(jianli): TO be removed from Stream API since it is never used.
        return new Bundle();
    }

    @Override
    public String getSavedInstanceStateString() {
        // TODO(jianli): Return saved scroll state as serializedf string;
        return "";
    }

    @Override
    public View getView() {
        return mRecyclerView;
    }

    @Override
    public void setHeaderViews(List<Header> headers) {
        ArrayList<View> headerViews = new ArrayList<View>();
        for (Header header : headers) {
            headerViews.add(header.getView());
        }
        mFeedStreamSurface.setHeaderViews(headerViews);
    }

    @Override
    public void setStreamContentVisibility(boolean visible) {
        if (visible == mIsStreamContentVisible) {
            return;
        }
        mIsStreamContentVisible = visible;
    }

    @Override
    public void trim() {
        mRecyclerView.getRecycledViewPool().clear();
    }

    @Override
    public void smoothScrollBy(int dx, int dy) {
        mRecyclerView.smoothScrollBy(dx, dy);
    }

    @Override
    public int getChildTopAt(int position) {
        if (!isChildAtPositionVisible(position)) {
            return POSITION_NOT_KNOWN;
        }

        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return POSITION_NOT_KNOWN;
        }

        View view = layoutManager.findViewByPosition(position);
        if (view == null) {
            return POSITION_NOT_KNOWN;
        }

        return view.getTop();
    }

    @Override
    public boolean isChildAtPositionVisible(int position) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }

        int firstItemPosition = layoutManager.findFirstVisibleItemPosition();
        int lastItemPosition = layoutManager.findLastVisibleItemPosition();
        if (firstItemPosition == RecyclerView.NO_POSITION
                || lastItemPosition == RecyclerView.NO_POSITION) {
            return false;
        }

        return position >= firstItemPosition && position <= lastItemPosition;
    }

    @Override
    public void addScrollListener(ScrollListener listener) {
        mScrollListeners.addObserver(listener);
    }

    @Override
    public void removeScrollListener(ScrollListener listener) {
        mScrollListeners.removeObserver(listener);
    }

    @Override
    public void addOnContentChangedListener(ContentChangedListener listener) {
        // Not longer needed.
    }

    @Override
    public void removeOnContentChangedListener(ContentChangedListener listener) {
        // Not longer needed.
    }

    @Override
    public void triggerRefresh() {}

    private void setupRecyclerView() {
        mRecyclerView = (RecyclerView) mFeedStreamSurface.getView();
        mRecyclerView.setId(R.id.feed_stream_recycler_view);
        mRecyclerView.setClipToPadding(false);
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView v, int x, int y) {
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrolled(x, y);
                }
            }
        });

        mFeedStreamSurface.surfaceOpened();
    }
}
