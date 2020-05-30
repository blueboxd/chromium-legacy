// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import android.view.View;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ViewActionManager;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.PietViewHolder;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;

/**
 * {@link DefaultItemAnimator} implementation that notifies the given {@link ContentChangedListener}
 * after animations occur.
 */
public class StreamItemAnimator extends DefaultItemAnimator {
    private final ContentChangedListener mContentChangedListener;
    private final ViewActionManager mViewActionManager;
    private boolean mIsStreamContentVisible;
    private RecyclerView mParent;

    public StreamItemAnimator(ContentChangedListener contentChangedListener,
            ViewActionManager viewActionManager, RecyclerView parent) {
        this.mContentChangedListener = contentChangedListener;
        this.mViewActionManager = viewActionManager;
        mParent = parent;
    }

    @Override
    public void onAnimationFinished(RecyclerView.ViewHolder viewHolder) {
        super.onAnimationFinished(viewHolder);
        mContentChangedListener.onContentChanged();
        if (this.mIsStreamContentVisible) mViewActionManager.onAnimationFinished();

        // Set recyclerView for Feed as visible when first patch of articles are loaded.
        if (viewHolder instanceof PietViewHolder && mParent != null
                && mParent.getVisibility() == View.INVISIBLE) {
            Animation animate = new AlphaAnimation(0, 1);
            animate.setInterpolator(new DecelerateInterpolator());
            animate.setDuration(700);
            mParent.startAnimation(animate);
            mParent.setVisibility(View.VISIBLE);
        }
    }

    public void setStreamVisibility(boolean isStreamContentVisible) {
        if (this.mIsStreamContentVisible == isStreamContentVisible) {
            return;
        }

        if (isStreamContentVisible) {
            // Ending animations so that if any content is animating out the RecyclerView will be
            // able to remove those views. This can occur if a user quickly presses hide and then
            // show on the stream.
            endAnimations();
        }

        this.mIsStreamContentVisible = isStreamContentVisible;
    }

    @VisibleForTesting
    public boolean getStreamContentVisibility() {
        return mIsStreamContentVisible;
    }
}
