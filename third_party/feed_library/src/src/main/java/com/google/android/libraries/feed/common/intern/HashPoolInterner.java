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

import android.support.v4.util.SparseArrayCompat;
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

    private final SparseArrayCompat<WeakReference<T>> pool = new SparseArrayCompat<>();

    @Override
    /*@Nullable*/
    public T get(T input) {
      WeakReference<T> weakRef = (input != null) ? pool.get(input.hashCode()) : null;
      return weakRef != null ? weakRef.get() : null;
    }

    @Override
    public void put(T input) {
      if (input != null) {
        pool.put(input.hashCode(), new WeakReference<>(input));
      }
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
