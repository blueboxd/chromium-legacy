// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * In order to support optimistic writes, the cache must store the payloads for the full duration of
 * the mutation. The cache supports lifecycle methods to start and finish the mutation. The cache
 * will hold onto payloads during the duration of the mutation, but will clear data when a mutation
 * is started or finished.
 *
 * <p>This cache assumes that we update all ModelProviders within the lifecycle of a mutation. This
 * is the current behavior of FeedModelProvider.
 */
public final class ContentCache implements Dumpable {
    private static final String TAG = "ContentCache";

    private final Map<String, StreamPayload> mutationCache;

    private int lookupCount;
    private int hitCount;
    private int maxMutationCacheSize;
    private int mutationsCount;

    public ContentCache() {
        mutationCache = new ConcurrentHashMap<>();
    }

    /**
     * Called when the {@link
     * com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager} commits a
     * new mutation. Everything added to the cache must be retained until {@link #finishMutation()}
     * is called.
     */
    void startMutation() {
        mutationsCount++;
        mutationCache.clear();
    }

    /**
     * Called when the {@link
     * com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager} has
     * finished the mutation. At this point it would be safe to clear the cache.
     */
    void finishMutation() {
        if (mutationCache.size() > maxMutationCacheSize) {
            maxMutationCacheSize = mutationCache.size();
        }
        mutationCache.clear();
    }

    /** Return the {@link StreamPayload} or {@code null} if it is not found in the cache. */
    /*@Nullable*/
    public StreamPayload get(String key) {
        StreamPayload value = mutationCache.get(key);
        lookupCount++;
        if (value != null) {
            hitCount++;
        } else {
            // This is expected on startup.
            Logger.d(TAG, "Mutation Cache didn't contain item %s", key);
        }
        return value;
    }

    /** Add a new value to the cache, returning the previous version or {@code null}. */
    /*@Nullable*/
    public StreamPayload put(String key, StreamPayload payload) {
        return mutationCache.put(key, payload);
    }

    /** Returns the current number of items in the cache. */
    public int size() {
        return mutationCache.size();
    }

    public void reset() {
        mutationCache.clear();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("mutationCacheSize").value(mutationCache.size());
        dumper.forKey("mutationsCount").value(mutationsCount).compactPrevious();
        dumper.forKey("maxMutationCacheSize").value(maxMutationCacheSize).compactPrevious();
        dumper.forKey("lookupCount").value(lookupCount);
        dumper.forKey("hits").value(hitCount).compactPrevious();
        dumper.forKey("misses").value(lookupCount - hitCount).compactPrevious();
    }
}
