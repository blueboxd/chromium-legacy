// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import androidx.annotation.Nullable;

/**
 * Interface defining a simple Pool of Adapters.
 *
 * @param <A> The adapter being managed by the {@link RecyclerPool}
 */
interface RecyclerPool<A extends ElementAdapter<?, ?>> {
    /**
     * Return an {@link ElementAdapter} matching the {@link RecyclerKey} or null if one isn't found.
     */
    @Nullable
    A get(RecyclerKey key);

    /** Put a {@link ElementAdapter} with a {@link RecyclerKey} into the pool. */
    void put(RecyclerKey key, A adapter);

    /** Clear everything out of the recycler pool. */
    void clear();
}
