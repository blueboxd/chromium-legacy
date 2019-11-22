// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal;

import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;

/**
 * {@link DefaultItemAnimator} implementation that notifies the given {@link ContentChangedListener}
 * after animations occur.
 */
public class StreamItemAnimator extends DefaultItemAnimator {
    private final ContentChangedListener contentChangedListener;
    private boolean isStreamContentVisible;

    public StreamItemAnimator(ContentChangedListener contentChangedListener) {
        this.contentChangedListener = contentChangedListener;
    }

    @Override
    public void onAnimationFinished(RecyclerView.ViewHolder viewHolder) {
        super.onAnimationFinished(viewHolder);
        contentChangedListener.onContentChanged();
    }

    public void setStreamVisibility(boolean isStreamContentVisible) {
        if (this.isStreamContentVisible == isStreamContentVisible) {
            return;
        }

        if (isStreamContentVisible) {
            // Ending animations so that if any content is animating out the RecyclerView will be
            // able to remove those views. This can occur if a user quickly presses hide and then
            // show on the stream.
            endAnimations();
        }

        this.isStreamContentVisible = isStreamContentVisible;
    }

    @VisibleForTesting
    public boolean getStreamContentVisibility() {
        return isStreamContentVisible;
    }
}
