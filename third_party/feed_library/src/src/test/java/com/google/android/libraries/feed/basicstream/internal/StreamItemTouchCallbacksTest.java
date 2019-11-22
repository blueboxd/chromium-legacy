// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.basicstream.internal.viewholders.SwipeableViewHolder;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link StreamItemTouchCallbacks}. */
@RunWith(RobolectricTestRunner.class)
public final class StreamItemTouchCallbacksTest {
    private final int NO_MOVEMENT_FLAG;

    private DismissibleViewHolder dismissibleViewHolder;

    private StreamItemTouchCallbacks callbacks;
    private FrameLayout frameLayout;
    private RecyclerView recyclerView;

    @Before
    public void setUp() {
        callbacks = new StreamItemTouchCallbacks();
        Context context = Robolectric.buildActivity(Activity.class).get();
        recyclerView = new RecyclerView(context);
        frameLayout = new FrameLayout(context);
        dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ false);
    }

    @Test
    public void testGetMovementFlags_nonSwipeableViewHolderType() {
        ViewHolder nonPietViewHolder = mock(ViewHolder.class);

        int movementFlags = callbacks.getMovementFlags(recyclerView, nonPietViewHolder);

        assertThat(movementFlags).isEqualTo(NO_MOVEMENT_FLAG);
    }

    @Test
    public void testGetMovementFlags_notDismissibleSwipeableViewholder() {
        dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ false);

        int movementFlags = callbacks.getMovementFlags(recyclerView, dismissibleViewHolder);

        assertThat(movementFlags).isEqualTo(NO_MOVEMENT_FLAG);
    }

    @Test
    public void testGetMovementFlags_swipeablePiet() {
        dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ true);

        int movementFlags = callbacks.getMovementFlags(recyclerView, dismissibleViewHolder);

        assertThat(movementFlags)
                .isEqualTo(ItemTouchHelper.Callback.makeMovementFlags(
                        0, ItemTouchHelper.START | ItemTouchHelper.END));
    }

    @Test
    public void testOnSwiped() {
        dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ true);
        assertThat(dismissibleViewHolder.isOnSwipedCalled()).isFalse();

        callbacks.onSwiped(dismissibleViewHolder, 0);

        assertThat(dismissibleViewHolder.isOnSwipedCalled()).isTrue();
    }

    @Test
    public void testOnChildDraw() {
        float translationX = 65;
        frameLayout.measure(MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
        dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ false);

        callbacks.onChildDraw(new Canvas(), recyclerView, dismissibleViewHolder, translationX,
                /* dY= */ 0,
                /* i= */ 1,
                /* isCurrentlyActive= */ true);

        assertThat(dismissibleViewHolder.itemView.getTranslationX()).isEqualTo(translationX);
        assertThat(dismissibleViewHolder.itemView.getAlpha()).isEqualTo(.5f);
    }

    private static class DismissibleViewHolder extends ViewHolder implements SwipeableViewHolder {
        private final boolean isDismissible;
        private boolean onSwipedCalled;

        DismissibleViewHolder(View view, boolean isDismissible) {
            super(view);
            this.isDismissible = isDismissible;
        }

        @Override
        public boolean canSwipe() {
            return isDismissible;
        }

        @Override
        public void onSwiped() {
            onSwipedCalled = true;
        }

        boolean isOnSwipedCalled() {
            return onSwipedCalled;
        }
    }
}
