// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.testing;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;

/**
 * Delegate class allowing spying on JournalStorageDirect delegates implementing both of the storage
 * interfaces.
 */
public class DelegatingJournalStorage implements JournalStorage, JournalStorageDirect {
    private final JournalStorageDirect mDelegate;

    public DelegatingJournalStorage(JournalStorageDirect delegate) {
        this.mDelegate = delegate;
    }

    @Override
    public void read(String journalName, Consumer<Result<List<byte[]>>> consumer) {
        consumer.accept(mDelegate.read(journalName));
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        return mDelegate.read(journalName);
    }

    @Override
    public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
        consumer.accept(mDelegate.commit(mutation));
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        return mDelegate.commit(mutation);
    }

    @Override
    public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
        consumer.accept(mDelegate.exists(journalName));
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        return mDelegate.exists(journalName);
    }

    @Override
    public void getAllJournals(Consumer<Result<List<String>>> consumer) {
        consumer.accept(mDelegate.getAllJournals());
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return mDelegate.getAllJournals();
    }

    @Override
    public void deleteAll(Consumer<CommitResult> consumer) {
        consumer.accept(mDelegate.deleteAll());
    }

    @Override
    public CommitResult deleteAll() {
        return mDelegate.deleteAll();
    }
}
