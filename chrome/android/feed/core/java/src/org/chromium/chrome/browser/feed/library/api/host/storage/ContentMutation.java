// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Delete;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.DeleteAll;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.DeleteByPrefix;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Upsert;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A sequence of {@link ContentOperation} instances. This class is used to commit changes to {@link
 * ContentStorage}.
 */
public final class ContentMutation {
    private final List<ContentOperation> mOperations;

    private ContentMutation(List<ContentOperation> operations) {
        this.mOperations = Collections.unmodifiableList(operations);
    }

    /** An unmodifiable list of operations to be committed by {@link ContentStorage}. */
    public List<ContentOperation> getOperations() {
        return mOperations;
    }

    /**
     * Creates a {@link ContentMutation}, which can be used to apply mutations to the underlying
     * {@link ContentStorage}.
     */
    public static final class Builder {
        private final ArrayList<ContentOperation> mOperations = new ArrayList<>();
        private final ContentOperationListSimplifier mSimplifier =
                new ContentOperationListSimplifier();

        /**
         * Sets the key/value pair in {@link ContentStorage}, or inserts it if it doesn't exist.
         *
         * <p>This method can be called repeatedly to assign multiple key/values. If the same key is
         * assigned multiple times, only the last value will be persisted.
         */
        public Builder upsert(String key, byte[] value) {
            mOperations.add(new Upsert(key, value));
            return this;
        }

        /**
         * Deletes the value from {@link ContentStorage} with a matching key, if it exists.
         *
         * <p>{@link ContentStorage#commit(ContentMutation)} will fulfill with result is {@code
         * TRUE}, even if {@code key} is not found in {@link ContentStorage}.
         *
         * <p>If {@link Delete} and {@link Upsert} are committed for the same key, only the last
         * operation will take effect.
         */
        public Builder delete(String key) {
            mOperations.add(new Delete(key));
            return this;
        }

        /**
         * Deletes all values from {@link ContentStorage} with matching key prefixes.
         *
         * <p>{@link ContentStorage#commit(ContentMutation)} will fulfill with result equal to
         * {@code TRUE}, even if no keys have a matching prefix.
         *
         * <p>If {@link DeleteByPrefix} and {@link Upsert} are committed for the same matching key,
         * only the last operation will take effect.
         */
        public Builder deleteByPrefix(String prefix) {
            mOperations.add(new DeleteByPrefix(prefix));
            return this;
        }

        /** Deletes all values from {@link ContentStorage}. */
        public Builder deleteAll() {
            mOperations.add(new DeleteAll());
            return this;
        }

        /**
         * Simplifies the sequence of {@link ContentOperation} instances, and returns a new {@link
         * ContentMutation} the simplified list.
         */
        public ContentMutation build() {
            return new ContentMutation(mSimplifier.simplify(mOperations));
        }
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }

        ContentMutation that = (ContentMutation) o;

        return mOperations != null ? mOperations.equals(that.mOperations)
                                   : that.mOperations == null;
    }

    @Override
    public int hashCode() {
        return mOperations != null ? mOperations.hashCode() : 0;
    }
}
