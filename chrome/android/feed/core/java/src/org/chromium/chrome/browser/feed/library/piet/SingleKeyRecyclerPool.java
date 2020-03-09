// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import androidx.annotation.Nullable;
import androidx.core.util.Pools.SimplePool;

/** A very simple, single pool version of a {@link RecyclerPool} */
class SingleKeyRecyclerPool<A extends ElementAdapter<?, ?>> implements RecyclerPool<A> {
    private static final String KEY_ERROR_MESSAGE = "Given key %s does not match singleton key %s";

    private final RecyclerKey mSingletonKey;
    private final int mCapacity;

    private SimplePool<A> mPool;

    SingleKeyRecyclerPool(RecyclerKey key, int capacity) {
        mSingletonKey = key;
        this.mCapacity = capacity;
        mPool = new SimplePool<>(capacity);
    }

    @Nullable
    @Override
    public A get(RecyclerKey key) {
        if (!mSingletonKey.equals(key)) {
            throw new IllegalArgumentException(
                    String.format(KEY_ERROR_MESSAGE, key, mSingletonKey));
        }
        return mPool.acquire();
    }

    @Override
    public void put(RecyclerKey key, A adapter) {
        if (key == null) {
            throw new NullPointerException(String.format("null key for %s", adapter));
        }
        if (!mSingletonKey.equals(key)) {
            throw new IllegalArgumentException(
                    String.format(KEY_ERROR_MESSAGE, key, mSingletonKey));
        }
        mPool.release(adapter);
    }

    @Override
    public void clear() {
        mPool = new SimplePool<>(mCapacity);
    }
}
