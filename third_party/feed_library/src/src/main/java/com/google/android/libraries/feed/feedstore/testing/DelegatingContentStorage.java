// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.testing;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;

import java.util.List;
import java.util.Map;

/**
 * Delegate class allowing spying on ContentStorageDirect delegates implementing both of the storage
 * interfaces.
 */
public class DelegatingContentStorage implements ContentStorage, ContentStorageDirect {
    private final ContentStorageDirect delegate;

    public DelegatingContentStorage(ContentStorageDirect delegate) {
        this.delegate = delegate;
    }

    @Override
    public void get(List<String> keys, Consumer<Result<Map<String, byte[]>>> consumer) {
        consumer.accept(delegate.get(keys));
    }

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        return delegate.get(keys);
    }

    @Override
    public void getAll(String prefix, Consumer<Result<Map<String, byte[]>>> consumer) {
        consumer.accept(delegate.getAll(prefix));
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        return delegate.getAll(prefix);
    }

    @Override
    public void getAllKeys(Consumer<Result<List<String>>> consumer) {
        consumer.accept(delegate.getAllKeys());
    }

    @Override
    public Result<List<String>> getAllKeys() {
        return delegate.getAllKeys();
    }

    @Override
    public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
        consumer.accept(delegate.commit(mutation));
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        return delegate.commit(mutation);
    }
}
