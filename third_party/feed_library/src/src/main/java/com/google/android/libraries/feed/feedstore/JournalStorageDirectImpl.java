// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalStorage;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadCaller;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;

import java.util.List;

/**
 * An implementation of {@link JournalStorageDirect} which converts {@link JournalStorage} to a
 * synchronized implementation. This acts as a wrapper class over JournalStorage. It will provide a
 * consumer calling on the main thread and waiting on a Future to complete to return the consumer
 * results.
 */
public final class JournalStorageDirectImpl
        extends MainThreadCaller implements JournalStorageDirect {
    private static final String LOCATION = "JournalStorage.";
    private final JournalStorage journalStorage;

    public JournalStorageDirectImpl(
            JournalStorage journalStorage, MainThreadRunner mainThreadRunner) {
        super(mainThreadRunner);
        this.journalStorage = journalStorage;
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        return mainThreadCaller(LOCATION + "read",
                (consumer) -> journalStorage.read(journalName, consumer), Result.failure());
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        return mainThreadCaller(LOCATION + "commit",
                (consumer) -> journalStorage.commit(mutation, consumer), CommitResult.FAILURE);
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        return mainThreadCaller(LOCATION + "exists",
                (consumer) -> journalStorage.exists(journalName, consumer), Result.failure());
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return mainThreadCaller(
                LOCATION + "getAllJournals", journalStorage::getAllJournals, Result.failure());
    }

    @Override
    public CommitResult deleteAll() {
        return mainThreadCaller(
                LOCATION + "deleteAll", journalStorage::deleteAll, CommitResult.FAILURE);
    }
}
