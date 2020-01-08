// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.functional;

import org.chromium.base.supplier.Supplier;

/** Utility methods for working with the {@link Supplier} class. */
public class Suppliers {
    /**
     * Instead of {@link #of(T)} returning a lambda, use this inner class to avoid making new
     * classes for each lambda.
     */
    private static class InstancesSupplier<T> implements Supplier<T> {
        private final T mInstance;

        InstancesSupplier(T instance) {
            this.mInstance = instance;
        }

        @Override
        public T get() {
            return mInstance;
        }
    }

    /** Return a {@link Supplier} that always returns the provided instance. */
    public static <T> Supplier<T> of(T instance) {
        return new InstancesSupplier<>(instance);
    }

    /** Prevent instantiation */
    private Suppliers() {}
}
