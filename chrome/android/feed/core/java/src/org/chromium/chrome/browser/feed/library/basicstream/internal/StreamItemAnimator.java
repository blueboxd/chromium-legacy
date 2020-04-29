// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ViewActionManager;

/**
 * {@link DefaultItemAnimator} implementation that notifies the given {@link ContentChangedListener}
 * after animations occur.
 */
public class StreamItemAnimator extends DefaultItemAnimator {
    private final ContentChangedListener mContentChangedListener;
    private final ViewActionManager mViewActionManager;
    private boolean mIsStreamContentVisible;

    public StreamItemAnimator(
            ContentChangedListener contentChangedListener, ViewActionManager viewActionManager) {
        this.mContentChangedListener = contentChangedListener;
        this.mViewActionManager = viewActionManager;
    }

    @Override
    public void onAnimationFinished(RecyclerView.ViewHolder viewHolder) {
        super.onAnimationFinished(viewHolder);
        mContentChangedListener.onContentChanged();
        if (this.mIsStreamContentVisible) mViewActionManager.onAnimationFinished();
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
