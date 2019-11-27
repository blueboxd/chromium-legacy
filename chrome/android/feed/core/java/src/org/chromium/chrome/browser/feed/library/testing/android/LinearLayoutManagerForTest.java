// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.android;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import java.util.HashMap;
import java.util.Map;

/** A {@link LinearLayoutManager} used for testing. */
public final class LinearLayoutManagerForTest extends LinearLayoutManager {
    private final Map<Integer, View> mChildMap;

    public int firstVisibleItemPosition = RecyclerView.NO_POSITION;
    public int firstVisibleViewOffset;

    public int lastVisibleItemPosition = RecyclerView.NO_POSITION;

    public int scrolledPosition;
    public int scrolledOffset;

    public LinearLayoutManagerForTest(Context context) {
        super(context);
        this.mChildMap = new HashMap<>();
    }

    @Override
    public int findFirstVisibleItemPosition() {
        return firstVisibleItemPosition;
    }

    @Override
    public int findLastVisibleItemPosition() {
        return lastVisibleItemPosition;
    }

    @Override
    public View findViewByPosition(int position) {
        if (mChildMap.containsKey(position)) {
            return checkNotNull(mChildMap.get(position),
                    "addChildToPosition(int position, View child) should be called prior to "
                            + "findViewByPosition(int position).");
        }
        return super.findViewByPosition(position);
    }

    @Override
    public void scrollToPositionWithOffset(int scrollPosition, int scrollOffset) {
        scrolledPosition = scrollPosition;
        scrolledOffset = scrollOffset;
    }

    public void addChildToPosition(int position, View child) {
        mChildMap.put(position, child);
    }
}
