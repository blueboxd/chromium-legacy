// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage.testing;

import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.APPEND;
import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.COPY;
import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.DELETE;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Append;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Copy;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** A {@link JournalStorage} implementation that holds the data in memory. */
public final class InMemoryJournalStorage implements JournalStorageDirect, Dumpable {
    private static final String TAG = "InMemoryJournalStorage";

    private final Map<String, List<byte[]>> mJournals;
    private int mReadCount;
    private int mAppendCount;
    private int mCopyCount;
    private int mDeleteCount;

    public InMemoryJournalStorage() {
        mJournals = new HashMap<>();
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        mReadCount++;
        List<byte[]> journal = mJournals.get(journalName);
        if (journal == null) {
            journal = new ArrayList<>();
        }
        return Result.success(journal);
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        String journalName = mutation.getJournalName();

        for (JournalOperation operation : mutation.getOperations()) {
            if (operation.getType() == APPEND) {
                if (!append(((Append) operation).getValue(), journalName)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == COPY) {
                if (!copy(journalName, ((Copy) operation).getToJournalName())) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE) {
                if (!delete(journalName)) {
                    return CommitResult.FAILURE;
                }
            } else {
                Logger.w(TAG, "Unexpected JournalOperation type: %s", operation.getType());
                return CommitResult.FAILURE;
            }
        }
        return CommitResult.SUCCESS;
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        return Result.success(mJournals.containsKey(journalName));
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return Result.success(new ArrayList<>(mJournals.keySet()));
    }

    @Override
    public CommitResult deleteAll() {
        mJournals.clear();
        return CommitResult.SUCCESS;
    }

    private boolean append(byte[] value, String journalName) {
        List<byte[]> journal = mJournals.get(journalName);
        if (value == null) {
            Logger.e(TAG, "Journal not found: %s", journalName);
            return false;
        }
        if (journal == null) {
            journal = new ArrayList<>();
            mJournals.put(journalName, journal);
        }
        mAppendCount++;
        journal.add(value);
        return true;
    }

    private boolean copy(String fromJournalName, String toJournalName) {
        mCopyCount++;
        List<byte[]> toJournal = mJournals.get(toJournalName);
        if (toJournal != null) {
            Logger.e(TAG, "Copy destination journal already present: %s", toJournalName);
            return false;
        }
        List<byte[]> journal = mJournals.get(fromJournalName);
        if (journal != null) {
            // TODO: Compact before copying?
            mJournals.put(toJournalName, new ArrayList<>(journal));
        }
        return true;
    }

    private boolean delete(String journalName) {
        mDeleteCount++;
        mJournals.remove(journalName);
        return true;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("readCount").value(mReadCount);
        dumper.forKey("appendCount").value(mAppendCount).compactPrevious();
        dumper.forKey("copyCount").value(mCopyCount).compactPrevious();
        dumper.forKey("deleteCount").value(mDeleteCount).compactPrevious();
        dumper.forKey("sessions").value(mJournals.size());
        for (Entry<String, List<byte[]>> entry : mJournals.entrySet()) {
            Dumper childDumper = dumper.getChildDumper();
            childDumper.title("Session");
            childDumper.forKey("name").value(entry.getKey());
            childDumper.forKey("operations").value(entry.getValue().size()).compactPrevious();
        }
    }
}
