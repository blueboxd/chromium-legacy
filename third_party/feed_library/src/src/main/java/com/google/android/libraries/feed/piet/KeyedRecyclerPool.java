// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.util.LruCache;

/**
 * This is a simple implementation of a recycling pool for Adapters.
 *
 * <p>TODO: This should be made real so it supports control of the pool size
 */
class KeyedRecyclerPool<A extends ElementAdapter<?, ?>> implements RecyclerPool<A> {
    private final LruCache<RecyclerKey, SingleKeyRecyclerPool<A>> poolMap;
    private final int capacityPerPool;

    KeyedRecyclerPool(int maxKeys, int capacityPerPool) {
        poolMap = new LruCache<>(maxKeys);
        this.capacityPerPool = capacityPerPool;
    }

    @Override
    /*@Nullable*/
    public A get(RecyclerKey key) {
        if (key == null) {
            return null;
        }
        SingleKeyRecyclerPool<A> pool = poolMap.get(key);
        if (pool == null) {
            return null;
        } else {
            return pool.get(key);
        }
    }

    @Override
    public void put(RecyclerKey key, A adapter) {
        checkNotNull(key, "null key for %s", adapter);
        SingleKeyRecyclerPool<A> pool = poolMap.get(key);
        if (pool == null) {
            pool = new SingleKeyRecyclerPool<>(key, capacityPerPool);
            poolMap.put(key, pool);
        }
        pool.put(key, adapter);
    }

    @Override
    public void clear() {
        poolMap.evictAll();
    }
}
