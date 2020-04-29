// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.actionmanager;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;

/** Manages the collection of View actions for cards in the feed. **/
public interface ViewActionManager {
    /**
     * Sets the container (usually a {@link android.view.ViewGroup}) representing the current
     * viewport.
     */
    public void setViewport(@Nullable View viewport);

    /** Signals that a view with associated reportable content has become visible. */
    public void onViewVisible(View view, String contentId, ActionPayload actionPayload);

    /** Signals that a view with associated reportable content has become hidden. */
    public void onViewHidden(View view, String contentId);

    /** Return a scroll listener that passes scroll state changes to this ViewReporter. */
    public ScrollListener getScrollListener();

    /**
     * Signal an {@link androidx.recyclerview.widget.RecyclerView.ItemAnimator#onAnimationFinished}
     * event at the stream level.
     */
    public void onAnimationFinished();

    /** Signal an {@link android.view.View.OnLayoutChangeListener#onLayoutChange} event. */
    public void onLayoutChange();

    /**
     * Signal an {@link org.chromium.chrome.browser.feed.library.api.client.stream.Stream#onShow}
     * event.
     */
    public void onShow();

    /**
     * Signal an {@link org.chromium.chrome.browser.feed.library.api.client.stream.Stream#onHide}
     * event.
     */
    public void onHide();

    /**
     * Asynchronously collect and store pending View actions, then call given Runnable.
     */
    public void storeViewActions(Runnable doneCallback);
}
