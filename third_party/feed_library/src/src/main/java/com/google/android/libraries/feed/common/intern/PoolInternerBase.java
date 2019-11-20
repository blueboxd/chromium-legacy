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
import javax.annotation.concurrent.ThreadSafe;

/**
 * An {@link Interner} base implementation using a given {@link Pool} to store interned objects.
 * Subclasses need to provide an actual {@link Pool} implementation.
 */
@ThreadSafe
public abstract class PoolInternerBase<T> implements Interner<T> {

  private final Pool<T> pool;

  PoolInternerBase(Pool<T> pool) {
    this.pool = Validators.checkNotNull(pool);
  }

  @Override
  public synchronized T intern(T input) {
    T output = pool.get(input);
    if (output != null) {
      return output;
    }

    pool.put(input);
    return input;
  }

  @Override
  public synchronized void clear() {
    pool.clear();
  }

  @Override
  public synchronized int size() {
    return pool.size();
  }

  /** Interface for a pool used by the PoolInternerBase. */
  protected interface Pool<T> {
    /** Retrieves the give object from the pool if it is found, or null otherwise. */
    /*@Nullable*/
    T get(T input);

    /** Stores the given object into the pool. */
    void put(T input);

    /** Clears the pool. */
    void clear();

    /** Returns the size of the pool. */
    int size();
  }
}
