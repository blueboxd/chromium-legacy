// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;
import java.util.Map;

/** A content storage API which is a synchronous implementation of {@link ContentStorage}. */
public interface ContentStorageDirect {
    /**
     * Requests the value for multiple keys. If a key does not have a value, it will not be included
     * in the map.
     */
    Result<Map<String, byte[]>> get(List<String> keys);

    /** Requests all key/value pairs from storage with a matching key prefix. */
    Result<Map<String, byte[]>> getAll(String prefix);

    /**
     * Commits the operations in the {@link ContentMutation} in order.
     *
     * <p>This operation is not guaranteed to be atomic. In the event of a failure, processing is
     * halted immediately, so the database may be left in an invalid state. Should this occur, Feed
     * behavior is undefined. Currently the plan is to wipe out existing data and start over.
     */
    CommitResult commit(ContentMutation mutation);

    /** Fetch all keys currently present in the content storage */
    Result<List<String>> getAllKeys();
}
