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

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;
import javax.annotation.concurrent.ThreadSafe;

/**
 * An {@link Interner} implementation using a WeakHashMap pool. Objects from the pool are
 * automatically garbage collected when there are no more references to them. Objects in the pool
 * are keyed by their own implementation so this interner is suitable for strings or relatively
 * small protos where the .equals() operation is not too expensive (see {@link WeakPoolInterner} for
 * an alternative implementation for that case).
 */
@ThreadSafe
public class WeakPoolInterner<T> extends PoolInternerBase<T> {

  public WeakPoolInterner() {
    super(new WeakPool<T>());
  }

  private static final class WeakPool<T> implements Pool<T> {

    private final WeakHashMap<T, WeakReference<T>> pool = new WeakHashMap<>();

    @Override
    /*@Nullable*/
    public T get(T input) {
      WeakReference<T> weakRef = pool.get(input);
      return weakRef != null ? weakRef.get() : null;
    }

    @Override
    public void put(T input) {
      pool.put(input, new WeakReference<>(input));
    }

    @Override
    public void clear() {
      pool.clear();
    }

    @Override
    public int size() {
      return pool.size();
    }
  }
}
