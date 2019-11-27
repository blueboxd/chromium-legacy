// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadCaller;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;

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
    private final JournalStorage mJournalStorage;

    public JournalStorageDirectImpl(
            JournalStorage journalStorage, MainThreadRunner mainThreadRunner) {
        super(mainThreadRunner);
        this.mJournalStorage = journalStorage;
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        return mainThreadCaller(LOCATION + "read",
                (consumer) -> mJournalStorage.read(journalName, consumer), Result.failure());
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        return mainThreadCaller(LOCATION + "commit",
                (consumer) -> mJournalStorage.commit(mutation, consumer), CommitResult.FAILURE);
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        return mainThreadCaller(LOCATION + "exists",
                (consumer) -> mJournalStorage.exists(journalName, consumer), Result.failure());
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return mainThreadCaller(
                LOCATION + "getAllJournals", mJournalStorage::getAllJournals, Result.failure());
    }

    @Override
    public CommitResult deleteAll() {
        return mainThreadCaller(
                LOCATION + "deleteAll", mJournalStorage::deleteAll, CommitResult.FAILURE);
    }
}
