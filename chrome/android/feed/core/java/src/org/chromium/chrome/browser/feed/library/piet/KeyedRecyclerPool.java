// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.util.LruCache;

/**
 * This is a simple implementation of a recycling pool for Adapters.
 *
 * <p>TODO: This should be made real so it supports control of the pool size
 */
class KeyedRecyclerPool<A extends ElementAdapter<?, ?>> implements RecyclerPool<A> {
    private final LruCache<RecyclerKey, SingleKeyRecyclerPool<A>> mPoolMap;
    private final int mCapacityPerPool;

    KeyedRecyclerPool(int maxKeys, int capacityPerPool) {
        mPoolMap = new LruCache<>(maxKeys);
        this.mCapacityPerPool = capacityPerPool;
    }

    @Override
    /*@Nullable*/
    public A get(RecyclerKey key) {
        if (key == null) {
            return null;
        }
        SingleKeyRecyclerPool<A> pool = mPoolMap.get(key);
        if (pool == null) {
            return null;
        } else {
            return pool.get(key);
        }
    }

    @Override
    public void put(RecyclerKey key, A adapter) {
        checkNotNull(key, "null key for %s", adapter);
        SingleKeyRecyclerPool<A> pool = mPoolMap.get(key);
        if (pool == null) {
            pool = new SingleKeyRecyclerPool<>(key, mCapacityPerPool);
            mPoolMap.put(key, pool);
        }
        pool.put(key, adapter);
    }

    @Override
    public void clear() {
        mPoolMap.evictAll();
    }
}
