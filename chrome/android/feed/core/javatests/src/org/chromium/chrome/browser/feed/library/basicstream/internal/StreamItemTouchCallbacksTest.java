// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.SwipeableViewHolder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link StreamItemTouchCallbacks}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StreamItemTouchCallbacksTest {
    private final int mNoMovementFlag = 0;

    private DismissibleViewHolder mDismissibleViewHolder;

    private StreamItemTouchCallbacks mCallbacks;
    private FrameLayout mFrameLayout;
    private RecyclerView mRecyclerView;

    @Before
    public void setUp() {
        mCallbacks = new StreamItemTouchCallbacks();
        Context context = Robolectric.buildActivity(Activity.class).get();
        mRecyclerView = new RecyclerView(context);
        mFrameLayout = new FrameLayout(context);
        mDismissibleViewHolder =
                new DismissibleViewHolder(mFrameLayout, /* isDismissible= */ false);
    }

    @Test
    public void testGetMovementFlags_nonSwipeableViewHolderType() {
        ViewHolder nonPietViewHolder = mock(ViewHolder.class);

        int movementFlags = mCallbacks.getMovementFlags(mRecyclerView, nonPietViewHolder);

        assertThat(movementFlags).isEqualTo(mNoMovementFlag);
    }

    @Test
    public void testGetMovementFlags_notDismissibleSwipeableViewholder() {
        mDismissibleViewHolder =
                new DismissibleViewHolder(mFrameLayout, /* isDismissible= */ false);

        int movementFlags = mCallbacks.getMovementFlags(mRecyclerView, mDismissibleViewHolder);

        assertThat(movementFlags).isEqualTo(mNoMovementFlag);
    }

    @Test
    public void testGetMovementFlags_swipeablePiet() {
        mDismissibleViewHolder = new DismissibleViewHolder(mFrameLayout, /* isDismissible= */ true);

        int movementFlags = mCallbacks.getMovementFlags(mRecyclerView, mDismissibleViewHolder);

        assertThat(movementFlags)
                .isEqualTo(ItemTouchHelper.Callback.makeMovementFlags(
                        0, ItemTouchHelper.START | ItemTouchHelper.END));
    }

    @Test
    public void testOnSwiped() {
        mDismissibleViewHolder = new DismissibleViewHolder(mFrameLayout, /* isDismissible= */ true);
        assertThat(mDismissibleViewHolder.isOnSwipedCalled()).isFalse();

        mCallbacks.onSwiped(mDismissibleViewHolder, 0);

        assertThat(mDismissibleViewHolder.isOnSwipedCalled()).isTrue();
    }

    @Test
    public void testOnChildDraw() {
        float translationX = 65;
        mFrameLayout.measure(MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
        mDismissibleViewHolder =
                new DismissibleViewHolder(mFrameLayout, /* isDismissible= */ false);

        mCallbacks.onChildDraw(new Canvas(), mRecyclerView, mDismissibleViewHolder, translationX,
                /* dY= */ 0,
                /* i= */ 1,
                /* isCurrentlyActive= */ true);

        assertThat(mDismissibleViewHolder.itemView.getTranslationX()).isEqualTo(translationX);
        assertThat(mDismissibleViewHolder.itemView.getAlpha()).isEqualTo(.5f);
    }

    private static class DismissibleViewHolder extends ViewHolder implements SwipeableViewHolder {
        private final boolean mIsDismissible;
        private boolean mOnSwipedCalled;

        DismissibleViewHolder(View view, boolean isDismissible) {
            super(view);
            this.mIsDismissible = isDismissible;
        }

        @Override
        public boolean canSwipe() {
            return mIsDismissible;
        }

        @Override
        public void onSwiped() {
            mOnSwipedCalled = true;
        }

        boolean isOnSwipedCalled() {
            return mOnSwipedCalled;
        }
    }
}
