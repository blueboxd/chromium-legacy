// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage.testing;

import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.DELETE;
import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.DELETE_BY_PREFIX;
import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.UPSERT;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Delete;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.DeleteByPrefix;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Upsert;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** A {@link ContentStorage} that holds data in memory. */
public final class InMemoryContentStorage implements ContentStorageDirect, Dumpable {
    private static final String TAG = "InMemoryContentStorage";

    private final Map<String, byte[]> mStore = new HashMap<>();

    private int mGetCount;
    private int mGetAllCount;
    private int mInsertCount;
    private int mUpdateCount;

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        mGetCount++;

        Map<String, byte[]> valueMap = new HashMap<>(keys.size());
        for (String key : keys) {
            byte[] value = mStore.get(key);
            if (value == null || value.length == 0) {
                Logger.w(TAG, "Didn't find value for %s, not adding to map", key);
            } else {
                valueMap.put(key, value);
            }
        }
        return Result.success(valueMap);
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        mGetAllCount++;
        Map<String, byte[]> results = new HashMap<>();
        for (Entry<String, byte[]> entry : mStore.entrySet()) {
            if (entry.getKey().startsWith(prefix)) {
                results.put(entry.getKey(), entry.getValue());
            }
        }
        return Result.success(results);
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        for (ContentOperation operation : mutation.getOperations()) {
            if (operation.getType() == UPSERT) {
                if (!upsert((Upsert) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE) {
                if (!delete((Delete) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE_BY_PREFIX) {
                if (!deleteByPrefix((DeleteByPrefix) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == Type.DELETE_ALL) {
                if (!deleteAll()) {
                    return CommitResult.FAILURE;
                }
            } else {
                Logger.e(TAG, "Invalid ContentMutation: unexpected operation: %s",
                        operation.getType());
                return CommitResult.FAILURE;
            }
        }

        return CommitResult.SUCCESS;
    }

    @Override
    public Result<List<String>> getAllKeys() {
        return Result.success(new ArrayList<>(mStore.keySet()));
    }

    private boolean deleteAll() {
        mStore.clear();
        return true;
    }

    private boolean deleteByPrefix(DeleteByPrefix operation) {
        List<String> keysToRemove = new ArrayList<>();
        for (String key : mStore.keySet()) {
            if (key.startsWith(operation.getPrefix())) {
                keysToRemove.add(key);
            }
        }
        mStore.keySet().removeAll(keysToRemove);
        return true;
    }

    private boolean delete(Delete operation) {
        mStore.remove(operation.getKey());
        return true;
    }

    private boolean upsert(Upsert operation) {
        String key = operation.getKey();
        // TODO: remove unneeded null checks.
        if (key == null) {
            Logger.e(TAG, "Invalid ContentMutation: null key");
            return false;
        }
        byte[] value = operation.getValue();
        if (value == null) {
            Logger.e(TAG, "Invalid ContentMutation: null value");
            return false;
        }
        if (value.length == 0) {
            Logger.e(TAG, "Invalid ContentMutation: empty value");
            return false;
        }
        byte[] currentValue = mStore.put(key, value);
        if (currentValue == null) {
            mInsertCount++;
        } else {
            mUpdateCount++;
        }
        return true;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("contentItems").value(mStore.size());
        dumper.forKey("getCount").value(mGetCount);
        dumper.forKey("getAllCount").value(mGetAllCount).compactPrevious();
        dumper.forKey("insertCount").value(mInsertCount).compactPrevious();
        dumper.forKey("updateCount").value(mUpdateCount).compactPrevious();
    }
}
