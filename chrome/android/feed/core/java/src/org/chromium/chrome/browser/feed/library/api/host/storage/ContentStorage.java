// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;
import java.util.Map;

/**
 * Allows reading and writing of content, currently key-value pairs.
 *
 * <p>Storage instances can be accessed from multiple threads.
 */
public interface ContentStorage {
    /**
     * Asynchronously requests the value for multiple keys. If a key does not have a value, it will
     * not be included in the map.
     */
    void get(List<String> keys, Consumer<Result<Map<String, byte[]>>> consumer);

    /** Asynchronously requests all key/value pairs from storage with a matching key prefix. */
    void getAll(String prefix, Consumer<Result<Map<String, byte[]>>> consumer);

    /**
     * Commits the operations in the {@link ContentMutation} in order and asynchronously reports the
     * {@link CommitResult}.
     *
     * <p>This operation is not guaranteed to be atomic. In the event of a failure, processing is
     * halted immediately, so the database may be left in an invalid state. Should this occur, Feed
     * behavior is undefined. Currently the plan is to wipe out existing data and start over.
     */
    void commit(ContentMutation mutation, Consumer<CommitResult> consumer);

    /** Fetch all keys currently present in the content storage */
    void getAllKeys(Consumer<Result<List<String>>> consumer);
}
