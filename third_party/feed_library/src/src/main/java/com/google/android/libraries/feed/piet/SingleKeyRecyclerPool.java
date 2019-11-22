// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import android.support.v4.util.Pools.SimplePool;

/** A very simple, single pool version of a {@link RecyclerPool} */
class SingleKeyRecyclerPool<A extends ElementAdapter<?, ?>> implements RecyclerPool<A> {
    private static final String KEY_ERROR_MESSAGE = "Given key %s does not match singleton key %s";

    private final RecyclerKey singletonKey;
    private final int capacity;

    private SimplePool<A> pool;

    SingleKeyRecyclerPool(RecyclerKey key, int capacity) {
        singletonKey = key;
        this.capacity = capacity;
        pool = new SimplePool<>(capacity);
    }

    /*@Nullable*/
    @Override
    public A get(RecyclerKey key) {
        if (!singletonKey.equals(key)) {
            throw new IllegalArgumentException(String.format(KEY_ERROR_MESSAGE, key, singletonKey));
        }
        return pool.acquire();
    }

    @Override
    public void put(RecyclerKey key, A adapter) {
        if (key == null) {
            throw new NullPointerException(String.format("null key for %s", adapter));
        }
        if (!singletonKey.equals(key)) {
            throw new IllegalArgumentException(String.format(KEY_ERROR_MESSAGE, key, singletonKey));
        }
        pool.release(adapter);
    }

    @Override
    public void clear() {
        pool = new SimplePool<>(capacity);
    }
}
