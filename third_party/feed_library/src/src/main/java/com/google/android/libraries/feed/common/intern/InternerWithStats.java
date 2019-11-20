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
import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;
import javax.annotation.concurrent.ThreadSafe;

/** Wrapper for {@link Interner} that also tracks cache hits/misses. */
public class InternerWithStats<T> implements Interner<T> {

  private final Interner<T> delegate;
  private final CacheStats stats = new CacheStats();

  public InternerWithStats(Interner<T> delegate) {
    this.delegate = Validators.checkNotNull(delegate);
  }

  @Override
  public T intern(T arg) {
    T internedArg = delegate.intern(arg);
    if (internedArg != arg) {
      stats.incrementHitCount();
    } else {
      stats.incrementMissCount();
    }
    return internedArg;
  }

  @Override
  public void clear() {
    delegate.clear();
    stats.reset();
  }

  @Override
  public int size() {
    return delegate.size();
  }

  public String getStats() {
    int hits = stats.hitCount();
    int total = hits + stats.missCount();
    double ratio = (total == 0) ? 1.0 : (double) hits / total;
    return String.format(Locale.US, "Cache Hit Ratio: %d/%d (%.2f)", hits, total, ratio);
  }

  /** Similar to Guava CacheStats but thread safe. */
  @ThreadSafe
  private static final class CacheStats {

    private final AtomicInteger hitCount = new AtomicInteger();
    private final AtomicInteger missCount = new AtomicInteger();

    private CacheStats() {}

    private void incrementHitCount() {
      hitCount.incrementAndGet();
    }

    private void incrementMissCount() {
      missCount.incrementAndGet();
    }

    private int hitCount() {
      return hitCount.get();
    }

    private int missCount() {
      return missCount.get();
    }

    public void reset() {
      hitCount.set(0);
      missCount.set(0);
    }
  }
}
