// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import org.chromium.chrome.browser.feed.library.common.Validators;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.ThreadSafe;

/** Wrapper for {@link Interner} that also tracks cache hits/misses. */
public class InternerWithStats<T> implements Interner<T> {
    private final Interner<T> mDelegate;
    private final CacheStats mStats = new CacheStats();

    public InternerWithStats(Interner<T> delegate) {
        this.mDelegate = Validators.checkNotNull(delegate);
    }

    @Override
    public T intern(T arg) {
        T internedArg = mDelegate.intern(arg);
        if (internedArg != arg) {
            mStats.incrementHitCount();
        } else {
            mStats.incrementMissCount();
        }
        return internedArg;
    }

    @Override
    public void clear() {
        mDelegate.clear();
        mStats.reset();
    }

    @Override
    public int size() {
        return mDelegate.size();
    }

    public String getStats() {
        int hits = mStats.hitCount();
        int total = hits + mStats.missCount();
        double ratio = (total == 0) ? 1.0 : (double) hits / total;
        return String.format(Locale.US, "Cache Hit Ratio: %d/%d (%.2f)", hits, total, ratio);
    }

    /** Similar to Guava CacheStats but thread safe. */
    @ThreadSafe
    private static final class CacheStats {
        private final AtomicInteger mHitCount = new AtomicInteger();
        private final AtomicInteger mMissCount = new AtomicInteger();

        private CacheStats() {}

        private void incrementHitCount() {
            mHitCount.incrementAndGet();
        }

        private void incrementMissCount() {
            mMissCount.incrementAndGet();
        }

        private int hitCount() {
            return mHitCount.get();
        }

        private int missCount() {
            return mMissCount.get();
        }

        public void reset() {
            mHitCount.set(0);
            mMissCount.set(0);
        }
    }
}
