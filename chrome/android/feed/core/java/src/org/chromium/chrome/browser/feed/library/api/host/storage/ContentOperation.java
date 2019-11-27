// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import android.support.annotation.IntDef;

import java.util.Arrays;

/** A mutation to the underlying {@link ContentStorage}. */
public abstract class ContentOperation {
    /** The types of operations. */
    @IntDef({Type.DELETE, Type.DELETE_BY_PREFIX, Type.UPSERT, Type.DELETE_ALL})
    public @interface Type {
        /** Delete the content for the provided key */
        int DELETE = 0;
        /** Delete all content with keys beginning with the specified prefix */
        int DELETE_BY_PREFIX = 1;
        /** Insert or update the content for the provided key */
        int UPSERT = 3;
        /** Delete all content for all keys. */
        int DELETE_ALL = 4;
    }

    public @Type int getType() {
        return mType;
    }

    private final @Type int mType;

    // Only the following classes may extend ContentOperation
    private ContentOperation(@Type int type) {
        this.mType = type;
    }

    /**
     * A {@link ContentOperation} created by calling {@link ContentMutation.Builder#upsert(String,
     * byte[])}.
     */
    public static final class Upsert extends ContentOperation {
        private final String mKey;
        private final byte[] mValue;

        Upsert(String key, byte[] value) {
            super(Type.UPSERT);
            this.mKey = key;
            this.mValue = value;
        }

        public String getKey() {
            return mKey;
        }

        public byte[] getValue() {
            return mValue;
        }

        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (!super.equals(o)) {
                return false;
            }

            if (o instanceof Upsert) {
                Upsert upsert = (Upsert) o;
                return mKey.equals(upsert.mKey) && Arrays.equals(mValue, upsert.mValue);
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            int result = mKey.hashCode();
            result = 31 * result + Arrays.hashCode(mValue);
            return result;
        }
    }

    /**
     * A {@link ContentOperation} created by calling {@link ContentMutation.Builder#delete(String)}.
     */
    public static final class Delete extends ContentOperation {
        private final String mKey;

        Delete(String key) {
            super(Type.DELETE);
            this.mKey = key;
        }

        public String getKey() {
            return mKey;
        }

        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (!super.equals(o)) {
                return false;
            }

            if (o instanceof Delete) {
                Delete delete = (Delete) o;
                return mKey.equals(delete.mKey);
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            return mKey.hashCode();
        }
    }

    /**
     * A {@link ContentOperation} created by calling {@link
     * ContentMutation.Builder#deleteByPrefix(String)}.
     */
    public static final class DeleteByPrefix extends ContentOperation {
        private final String mPrefix;

        DeleteByPrefix(String prefix) {
            super(Type.DELETE_BY_PREFIX);
            this.mPrefix = prefix;
        }

        public String getPrefix() {
            return mPrefix;
        }

        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (!super.equals(o)) {
                return false;
            }

            if (o instanceof DeleteByPrefix) {
                DeleteByPrefix that = (DeleteByPrefix) o;
                return mPrefix.equals(that.mPrefix);
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            return mPrefix.hashCode();
        }
    }

    // TODO: replace these comments with better details
    /** A {@link ContentOperation} created by calling {@link ContentMutation.Builder#deleteAll()} */
    static final class DeleteAll extends ContentOperation {
        DeleteAll() {
            super(Type.DELETE_ALL);
        }
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof ContentOperation)) {
            return false;
        }

        ContentOperation operation = (ContentOperation) o;

        return mType == operation.mType;
    }

    @Override
    public int hashCode() {
        return mType;
    }
}
