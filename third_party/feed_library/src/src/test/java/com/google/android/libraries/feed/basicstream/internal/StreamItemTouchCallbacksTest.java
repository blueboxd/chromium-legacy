// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  private final int NO_MOVEMENT_FLAG = 0;

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
        .isEqualTo(
            ItemTouchHelper.Callback.makeMovementFlags(
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
    frameLayout.measure(
        MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY),
        MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
    dismissibleViewHolder = new DismissibleViewHolder(frameLayout, /* isDismissible= */ false);

    callbacks.onChildDraw(
        new Canvas(),
        recyclerView,
        dismissibleViewHolder,
        translationX,
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
