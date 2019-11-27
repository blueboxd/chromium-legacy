// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;

/**
 * An AdapterFactory manages binding a model to an Adapter and releasing it once we are done with
 * the view. This provides the basic support for Recycling Adapters and Views. The {@link
 * AdapterKeySupplier} is provided by the {@link ElementAdapter} to supply all the information
 * needed needed by this class.
 *
 * @param <A> A subclass of {@link ElementAdapter} which this factory is managing.
 * @param <M> The model. This is the model type which is bound to the view by the adapter.
 */
class AdapterFactory<A extends ElementAdapter<?, M>, M> {
    private static final String TAG = "AdapterFactory";

    // TODO: Make these configurable instead of constants.
    private static final int DEFAULT_POOL_SIZE = 100;
    private static final int DEFAULT_NUM_POOLS = 10;

    private final Context mContext;
    private final AdapterParameters mParameters;
    private final AdapterKeySupplier<A, M> mKeySupplier;
    private final Statistics mStatistics;
    private final RecyclerPool<A> mRecyclingPool;

    /** Provides Adapter class level details to the factory. */
    interface AdapterKeySupplier<A extends ElementAdapter<?, M>, M> {
        /**
         * Returns a String tag for the Adapter. This will be used in messages and developer
         * logging
         */
        String getAdapterTag();

        /** Returns a new Adapter. */
        A getAdapter(Context context, AdapterParameters parameters);

        /** Returns the Key based upon the model. */
        RecyclerKey getKey(FrameContext frameContext, M model);
    }

    /** Key supplier with a singleton key. */
    abstract static class SingletonKeySupplier<A extends ElementAdapter<?, M>, M>
            implements AdapterKeySupplier<A, M> {
        public static final RecyclerKey SINGLETON_KEY = new RecyclerKey();

        @Override
        public RecyclerKey getKey(FrameContext frameContext, M model) {
            return SINGLETON_KEY;
        }
    }

    // SuppressWarnings for various errors. We don't want to have to import checker framework and
    // checker doesn't like generic assignments.
    @SuppressWarnings("nullness")
    AdapterFactory(
            Context context, AdapterParameters parameters, AdapterKeySupplier<A, M> keySupplier) {
        this.mContext = context;
        this.mParameters = parameters;
        this.mKeySupplier = keySupplier;
        this.mStatistics = new Statistics(keySupplier.getAdapterTag());
        if (keySupplier instanceof SingletonKeySupplier) {
            mRecyclingPool = new SingleKeyRecyclerPool<>(
                    SingletonKeySupplier.SINGLETON_KEY, DEFAULT_POOL_SIZE);
        } else {
            mRecyclingPool = new KeyedRecyclerPool<>(DEFAULT_NUM_POOLS, DEFAULT_POOL_SIZE);
        }
    }

    /** Returns an adapter suitable for binding the given model. */
    public A get(M model, FrameContext frameContext) {
        mStatistics.mGetCalls++;
        A a = mRecyclingPool.get(mKeySupplier.getKey(frameContext, model));
        if (a == null) {
            a = mKeySupplier.getAdapter(mContext, mParameters);
            mStatistics.mAdapterCreation++;
        } else {
            mStatistics.mPoolHit++;
        }
        return a;
    }

    /** Release the Adapter, releases the model and will recycle the Adapter */
    public void release(A a) {
        mStatistics.mReleaseCalls++;
        a.releaseAdapter();
        RecyclerKey key = a.getKey();
        if (key != null) {
            mRecyclingPool.put(key, a);
        }
    }

    public void purgeRecyclerPool() {
        mRecyclingPool.clear();
    }

    /**
     * Basic statistics about hits, creations, etc. used to track how get/release are being used.
     */
    static class Statistics {
        final String mFactoryName;
        int mAdapterCreation;
        int mPoolHit;
        int mReleaseCalls;
        int mGetCalls;

        public Statistics(String factoryName) {
            this.mFactoryName = factoryName;
        }

        @Override
        public String toString() {
            // String used to show statistics during debugging in Android Studio.
            return "Stats: " + mFactoryName + ", Hits:" + mPoolHit + ", creations "
                    + mAdapterCreation + ", Release: " + mReleaseCalls;
        }
    }
}
