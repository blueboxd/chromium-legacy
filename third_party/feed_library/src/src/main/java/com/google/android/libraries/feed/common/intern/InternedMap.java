// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  public synchronized int size() {
    return delegate.size();
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
  public synchronized V get(/*@Nullable*/ Object key) {
    return delegate.get(key);
  }

  @Override
  /*@Nullable*/
  public synchronized V put(K key, V value) {
    return delegate.put(key, interner.intern(value));
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
  public synchronized void clear() {
    delegate.clear();
    interner.clear();
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
