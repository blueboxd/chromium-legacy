// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import androidx.annotation.Nullable;
import androidx.collection.SparseArrayCompat;

import java.lang.ref.WeakReference;

import javax.annotation.concurrent.ThreadSafe;

/**
 * An {@link Interner} implementation using weak references for the values and object hashcodes for
 * the keys. As compared to WeakPoolInterner this interner will only automatically garbage collect
 * values, not the keys. This interner version is suitable for very large protos where running a
 * .equals() is expensive compared with .hashCode().
 */
@ThreadSafe
public class HashPoolInterner<T> extends PoolInternerBase<T> {
    public HashPoolInterner() {
        super(new HashPool<>());
    }

    private static final class HashPool<T> implements Pool<T> {
        private final SparseArrayCompat<WeakReference<T>> mPool = new SparseArrayCompat<>();

        @Override
        @Nullable
        public T get(T input) {
            WeakReference<T> weakRef = (input != null) ? mPool.get(input.hashCode()) : null;
            return weakRef != null ? weakRef.get() : null;
        }

        @Override
        public void put(T input) {
            if (input != null) {
                mPool.put(input.hashCode(), new WeakReference<>(input));
            }
        }

        @Override
        public void clear() {
            mPool.clear();
        }

        @Override
        public int size() {
            return mPool.size();
        }
    }
}
