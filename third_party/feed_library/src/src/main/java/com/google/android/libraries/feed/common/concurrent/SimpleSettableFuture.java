// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent;

import com.google.android.libraries.feed.common.Validators;

import java.util.concurrent.CancellationException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Simple future that allows for setting a value from another thread */
public final class SimpleSettableFuture<V> implements Future<V> {
    private final CountDownLatch latch = new CountDownLatch(1);
    private boolean cancelled;
    /*@Nullable*/ private V value;
    /*@Nullable*/ private Exception exception;

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
        latch.countDown();
        cancelled = true;
        return true;
    }

    @Override
    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public boolean isDone() {
        return latch.getCount() == 0;
    }

    @Override
    public V get() throws InterruptedException, ExecutionException {
        latch.await();
        if (isCancelled()) {
            throw new ExecutionException(new CancellationException());
        }
        if (exception != null) {
            throw new ExecutionException(exception);
        }
        return Validators.checkNotNull(value, "Unable to return null from a Future.get()");
    }

    @Override
    public V get(long timeout, TimeUnit unit)
            throws InterruptedException, ExecutionException, TimeoutException {
        boolean timedOut = !latch.await(timeout, unit);
        if (timedOut) {
            throw new TimeoutException();
        }
        if (isCancelled()) {
            throw new ExecutionException(new CancellationException());
        }
        if (exception != null) {
            throw new ExecutionException(exception);
        }
        return Validators.checkNotNull(value, "Unable to return null from a Future.get()");
    }

    public void put(V value) {
        if (!isDone()) {
            this.value = value;
            latch.countDown();
        }
    }

    public void putException(Exception e) {
        if (!isDone()) {
            this.exception = e;
            latch.countDown();
        }
    }
}
