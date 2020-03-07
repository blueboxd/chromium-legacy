// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.shadows;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Shadow for {@link RecyclerView.RecycledViewPool} which is able to give information on when the
 * pool has been cleared.
 *
 * <p>{@link ViewHolder#getItemViewType()} is used as the keys for pools; however, this field is
 * final. That means a special adapters needs to be created in order to handle all this. This shadow
 * helps so all this infrastructure isn't needed.
 */
@Implements(RecyclerView.RecycledViewPool.class)
public class ShadowRecycledViewPool {
    private int mClearCount;

    @Implementation
    public void clear() {
        mClearCount++;
    }

    public int getClearCallCount() {
        return mClearCount;
    }
}
