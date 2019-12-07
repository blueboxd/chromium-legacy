// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.Validators;

import java.util.concurrent.CancellationException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Simple future that allows for setting a value from another thread */
public final class SimpleSettableFuture<V> implements Future<V> {
    private final CountDownLatch mLatch = new CountDownLatch(1);
    private boolean mCancelled;
    @Nullable
    private V mValue;
    @Nullable
    private Exception mException;

    /** Note that this will not interrupt tasks as tasks aren't running within the future */
    @Override
    public boolean cancel(boolean mayInterruptIfRunning) {
        // If already cancelled, return true;
        if (isCancelled()) {
            return true;
        }

        // If already complete, cannot cancel
        if (isDone()) {
            return false;
        }

        // Count down latch (to free the future up)
        mLatch.countDown();
        mCancelled = true;
        return true;
    }

    @Override
    public boolean isCancelled() {
        return mCancelled;
    }

    @Override
    public boolean isDone() {
        return mLatch.getCount() == 0;
    }

    @Override
    public V get() throws InterruptedException, ExecutionException {
        mLatch.await();
        if (isCancelled()) {
            throw new ExecutionException(new CancellationException());
        }
        if (mException != null) {
            throw new ExecutionException(mException);
        }
        return Validators.checkNotNull(mValue, "Unable to return null from a Future.get()");
    }

    @Override
    public V get(long timeout, TimeUnit unit)
            throws InterruptedException, ExecutionException, TimeoutException {
        boolean timedOut = !mLatch.await(timeout, unit);
        if (timedOut) {
            throw new TimeoutException();
        }
        if (isCancelled()) {
            throw new ExecutionException(new CancellationException());
        }
        if (mException != null) {
            throw new ExecutionException(mException);
        }
        return Validators.checkNotNull(mValue, "Unable to return null from a Future.get()");
    }

    public void put(V value) {
        if (!isDone()) {
            this.mValue = value;
            mLatch.countDown();
        }
    }

    public void putException(Exception e) {
        if (!isDone()) {
            this.mException = e;
            mLatch.countDown();
        }
    }
}
