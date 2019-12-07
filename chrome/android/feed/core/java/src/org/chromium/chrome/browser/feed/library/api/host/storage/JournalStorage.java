// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;

/**
 * Allows reading and writing to an append-only storage medium.
 *
 * <p>Storage instances can be accessed from multiple threads.
 *
 * <p>[INTERNAL LINK]
 */
public interface JournalStorage {
    /**
     * Reads the journal and asynchronously returns the contents.
     *
     * <p>Reads on journals that do not exist will fulfill with an empty list.
     */
    void read(String journalName, Consumer<Result<List<byte[]>>> consumer);

    /**
     * Commits the operations in {@link JournalMutation} in order and asynchronously reports the
     * {@link CommitResult}. If all the operations succeed, {@code callback} is called with a
     * success result. If any operation fails, {@code callback} is called with a failure result and
     * the remaining operations are not processed.
     *
     * <p>This operation is not guaranteed to be atomic.
     */
    void commit(JournalMutation mutation, Consumer<CommitResult> consumer);

    /** Determines whether a journal exists and asynchronously responds. */
    void exists(String journalName, Consumer<Result<Boolean>> consumer);

    /** Asynchronously retrieve a list of all current journals */
    void getAllJournals(Consumer<Result<List<String>>> consumer);

    /** Delete all journals. Reports success or failure with the {@code consumer}. */
    void deleteAll(Consumer<CommitResult> consumer);
}
