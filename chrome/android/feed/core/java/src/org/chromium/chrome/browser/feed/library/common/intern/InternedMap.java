// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.Validators;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

/** Map wrapper that interns the values on put operations, using the given interner. */
public class InternedMap<K, V> implements Map<K, V> {
    private final Map<K, V> mDelegate;
    private final Interner<V> mInterner;

    public InternedMap(Map<K, V> delegate, Interner<V> interner) {
        this.mDelegate = Validators.checkNotNull(delegate);
        this.mInterner = Validators.checkNotNull(interner);
    }

    @Override
    public int size() {
        synchronized (mDelegate) {
            return mDelegate.size();
        }
    }

    @Override
    public boolean isEmpty() {
        return mDelegate.isEmpty();
    }

    @Override
    public boolean containsKey(@Nullable Object key) {
        return mDelegate.containsKey(key);
    }

    @Override
    public boolean containsValue(@Nullable Object value) {
        return mDelegate.containsValue(value);
    }

    @Override
    @Nullable
    public V get(@Nullable Object key) {
        synchronized (mDelegate) {
            return mDelegate.get(key);
        }
    }

    @Override
    @Nullable
    public V put(K key, V value) {
        synchronized (mDelegate) {
            return mDelegate.put(key, mInterner.intern(value));
        }
    }

    @Override
    @Nullable
    public V remove(@Nullable Object key) {
        return mDelegate.remove(key);
    }

    @Override
    public void putAll(Map<? extends K, ? extends V> m) {
        for (Map.Entry<? extends K, ? extends V> e : m.entrySet()) {
            K key = e.getKey();
            V value = e.getValue();
            put(key, value);
        }
    }

    @Override
    public void clear() {
        synchronized (mDelegate) {
            mDelegate.clear();
            mInterner.clear();
        }
    }

    @Override
    public Set<K> keySet() {
        return mDelegate.keySet();
    }

    @Override
    public Collection<V> values() {
        return mDelegate.values();
    }

    @Override
    public Set<Entry<K, V>> entrySet() {
        return mDelegate.entrySet();
    }
}
