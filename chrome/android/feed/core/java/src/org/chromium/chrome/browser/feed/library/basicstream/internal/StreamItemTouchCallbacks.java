// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import android.graphics.Canvas;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.animation.Interpolator;

import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.SwipeableViewHolder;

/** {@link ItemTouchHelper.Callback} to allow dismissing cards via swiping. */
public final class StreamItemTouchCallbacks extends ItemTouchHelper.Callback {
    private static final Interpolator DISMISS_INTERPOLATOR = new FastOutLinearInInterpolator();

    @Override
    public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
        int swipeFlags = 0;
        if (viewHolder instanceof SwipeableViewHolder
                && ((SwipeableViewHolder) viewHolder).canSwipe()) {
            swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
        }
        return makeMovementFlags(/* dragFlags= */ 0, swipeFlags);
    }

    @Override
    public boolean onMove(
            RecyclerView recyclerView, ViewHolder viewHolder, ViewHolder viewHolder1) {
        throw new AssertionError("onMove not supported."); // Drag and drop not supported, the
                                                           // method will never be called.
    }

    @Override
    public void onSwiped(ViewHolder viewHolder, int i) {
        // Only PietViewHolders support swipe to dismiss. If new ViewHolders support swipe to
        // dismiss, this should instead use an interface.
        ((SwipeableViewHolder) viewHolder).onSwiped();
    }

    @Override
    public void onChildDraw(Canvas canvas, RecyclerView recyclerView, ViewHolder viewHolder,
            float dX, float dY, int i, boolean isCurrentlyActive) {
        float input = Math.abs(dX) / viewHolder.itemView.getMeasuredWidth();
        float alpha = 1 - DISMISS_INTERPOLATOR.getInterpolation(input);

        viewHolder.itemView.setTranslationX(dX);
        viewHolder.itemView.setAlpha(alpha);
    }
}
