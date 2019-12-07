// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.Validators;

import javax.annotation.concurrent.ThreadSafe;

/**
 * An {@link Interner} base implementation using a given {@link Pool} to store interned objects.
 * Subclasses need to provide an actual {@link Pool} implementation.
 */
@ThreadSafe
public abstract class PoolInternerBase<T> implements Interner<T> {
    private final Pool<T> mPool;

    PoolInternerBase(Pool<T> pool) {
        this.mPool = Validators.checkNotNull(pool);
    }

    @Override
    public T intern(T input) {
        synchronized (mPool) {
            T output = mPool.get(input);
            if (output != null) {
                return output;
            }

            mPool.put(input);
            return input;
        }
    }

    @Override
    public void clear() {
        synchronized (mPool) {
            mPool.clear();
        }
    }

    @Override
    public int size() {
        synchronized (mPool) {
            return mPool.size();
        }
    }

    /** Interface for a pool used by the PoolInternerBase. */
    protected interface Pool<T> {
        /** Retrieves the give object from the pool if it is found, or null otherwise. */
        @Nullable
        T get(T input);

        /** Stores the given object into the pool. */
        void put(T input);

        /** Clears the pool. */
        void clear();

        /** Returns the size of the pool. */
        int size();
    }
}
