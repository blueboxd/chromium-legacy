// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.intern;

import com.google.android.libraries.feed.common.Validators;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

/** Map wrapper that interns the values on put operations, using the given interner. */
public class InternedMap<K, V> implements Map<K, V> {
    private final Map<K, V> delegate;
    private final Interner<V> interner;

    public InternedMap(Map<K, V> delegate, Interner<V> interner) {
        this.delegate = Validators.checkNotNull(delegate);
        this.interner = Validators.checkNotNull(interner);
    }

    @Override
    public int size() {
        synchronized (delegate) {
            return delegate.size();
        }
    }

    @Override
    public boolean isEmpty() {
        return delegate.isEmpty();
    }

    @Override
    public boolean containsKey(/*@Nullable*/ Object key) {
        return delegate.containsKey(key);
    }

    @Override
    public boolean containsValue(/*@Nullable*/ Object value) {
        return delegate.containsValue(value);
    }

    @Override
    /*@Nullable*/
    public V get(/*@Nullable*/ Object key) {
        synchronized (delegate) {
            return delegate.get(key);
        }
    }

    @Override
    /*@Nullable*/
    public V put(K key, V value) {
        synchronized (delegate) {
            return delegate.put(key, interner.intern(value));
        }
    }

    @Override
    /*@Nullable*/
    public V remove(/*@Nullable*/ Object key) {
        return delegate.remove(key);
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
        synchronized (delegate) {
            delegate.clear();
            interner.clear();
        }
    }

    @Override
    public Set<K> keySet() {
        return delegate.keySet();
    }

    @Override
    public Collection<V> values() {
        return delegate.values();
    }

    @Override
    public Set<Entry<K, V>> entrySet() {
        return delegate.entrySet();
    }
}
