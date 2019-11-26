// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadCaller;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;

import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * An implementation of {@link ContentStorageDirect} which converts {@link ContentStorage} to a
 * synchronized implementation. This acts as a wrapper class over ContentStorage. It will provide a
 * consumer calling on the main thread and waiting on a Future to complete to return the consumer
 * results.
 */
public final class ContentStorageDirectImpl
        extends MainThreadCaller implements ContentStorageDirect {
    private static final String LOCATION = "ContentStorage.";
    private final ContentStorage mContentStorage;

    public ContentStorageDirectImpl(
            ContentStorage contentStorage, MainThreadRunner mainThreadRunner) {
        super(mainThreadRunner);
        this.mContentStorage = contentStorage;
    }

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        if (keys.isEmpty()) {
            return Result.success(Collections.emptyMap());
        }

        return mainThreadCaller(LOCATION + "get",
                (consumer) -> mContentStorage.get(keys, consumer), Result.failure());
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        return mainThreadCaller(LOCATION + "getAll",
                (consumer) -> mContentStorage.getAll(prefix, consumer), Result.failure());
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        return mainThreadCaller(LOCATION + "commit",
                (consumer) -> mContentStorage.commit(mutation, consumer), CommitResult.FAILURE);
    }

    @Override
    public Result<List<String>> getAllKeys() {
        return mainThreadCaller(
                LOCATION + "getAllKeys", mContentStorage::getAllKeys, Result.failure());
    }
}
