// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

/** Interface that {@link ViewHolder} instances can implement to enable swipe functionality. */
public interface SwipeableViewHolder {
    /** Determines whether this {@link ViewHolder} can have a swipe performed on it. */
    boolean canSwipe();

    /** Called when this {@link ViewHolder} is swiped. */
    void onSwiped();
}
