// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;

/** Define a version of the {@link JournalStorage} which runs synchronously. */
public interface JournalStorageDirect {
    /**
     * Reads the journal and a returns the contents.
     *
     * <p>Reads on journals that do not exist will fulfill with an empty list.
     */
    Result<List<byte[]>> read(String journalName);

    /**
     * Commits the operations in {@link JournalMutation} in order and reports the {@link
     * CommitResult}. If all the operations succeed returns a success result, otherwise reports a
     * failure.
     *
     * <p>This operation is not guaranteed to be atomic.
     */
    CommitResult commit(JournalMutation mutation);

    /** Determines whether a journal exists. */
    Result<Boolean> exists(String journalName);

    /** Retrieve a list of all current journals */
    Result<List<String>> getAllJournals();

    /** Delete all journals. */
    CommitResult deleteAll();
}
