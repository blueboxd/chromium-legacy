// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

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

    private final Map<String, StreamPayload> mMutationCache;

    private int mLookupCount;
    private int mHitCount;
    private int mMaxMutationCacheSize;
    private int mMutationsCount;

    public ContentCache() {
        mMutationCache = Collections.synchronizedMap(new HashMap<>());
    }

    /**
     * Called when the {@link
     * org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager}
     * commits a new mutation. Everything added to the cache must be retained until {@link
     * #finishMutation()} is called.
     */
    void startMutation() {
        mMutationsCount++;
        mMutationCache.clear();
    }

    /**
     * Called when the {@link
     * org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager} has
     * finished the mutation. At this point it would be safe to clear the cache.
     */
    void finishMutation() {
        if (mMutationCache.size() > mMaxMutationCacheSize) {
            mMaxMutationCacheSize = mMutationCache.size();
        }
        mMutationCache.clear();
    }

    /** Return the {@link StreamPayload} or {@code null} if it is not found in the cache. */
    /*@Nullable*/
    public StreamPayload get(String key) {
        StreamPayload value = mMutationCache.get(key);
        mLookupCount++;
        if (value != null) {
            mHitCount++;
        } else {
            // This is expected on startup.
            Logger.d(TAG, "Mutation Cache didn't contain item %s", key);
        }
        return value;
    }

    /** Add a new value to the cache, returning the previous version or {@code null}. */
    /*@Nullable*/
    public StreamPayload put(String key, StreamPayload payload) {
        return mMutationCache.put(key, payload);
    }

    /** Returns the current number of items in the cache. */
    public int size() {
        return mMutationCache.size();
    }

    public void reset() {
        mMutationCache.clear();
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("mutationCacheSize").value(mMutationCache.size());
        dumper.forKey("mutationsCount").value(mMutationsCount).compactPrevious();
        dumper.forKey("maxMutationCacheSize").value(mMaxMutationCacheSize).compactPrevious();
        dumper.forKey("lookupCount").value(mLookupCount);
        dumper.forKey("hits").value(mHitCount).compactPrevious();
        dumper.forKey("misses").value(mLookupCount - mHitCount).compactPrevious();
    }
}
