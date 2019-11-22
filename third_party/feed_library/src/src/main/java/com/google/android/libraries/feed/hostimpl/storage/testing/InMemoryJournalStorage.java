// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage.testing;

import static com.google.android.libraries.feed.api.host.storage.JournalOperation.Type.APPEND;
import static com.google.android.libraries.feed.api.host.storage.JournalOperation.Type.COPY;
import static com.google.android.libraries.feed.api.host.storage.JournalOperation.Type.DELETE;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation.Append;
import com.google.android.libraries.feed.api.host.storage.JournalOperation.Copy;
import com.google.android.libraries.feed.api.host.storage.JournalStorage;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** A {@link JournalStorage} implementation that holds the data in memory. */
public final class InMemoryJournalStorage implements JournalStorageDirect, Dumpable {
    private static final String TAG = "InMemoryJournalStorage";

    private final Map<String, List<byte[]>> journals;
    private int readCount;
    private int appendCount;
    private int copyCount;
    private int deleteCount;

    public InMemoryJournalStorage() {
        journals = new HashMap<>();
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        readCount++;
        List<byte[]> journal = journals.get(journalName);
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
        return Result.success(journals.containsKey(journalName));
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return Result.success(new ArrayList<>(journals.keySet()));
    }

    @Override
    public CommitResult deleteAll() {
        journals.clear();
        return CommitResult.SUCCESS;
    }

    private boolean append(byte[] value, String journalName) {
        List<byte[]> journal = journals.get(journalName);
        if (value == null) {
            Logger.e(TAG, "Journal not found: %s", journalName);
            return false;
        }
        if (journal == null) {
            journal = new ArrayList<>();
            journals.put(journalName, journal);
        }
        appendCount++;
        journal.add(value);
        return true;
    }

    private boolean copy(String fromJournalName, String toJournalName) {
        copyCount++;
        List<byte[]> toJournal = journals.get(toJournalName);
        if (toJournal != null) {
            Logger.e(TAG, "Copy destination journal already present: %s", toJournalName);
            return false;
        }
        List<byte[]> journal = journals.get(fromJournalName);
        if (journal != null) {
            // TODO: Compact before copying?
            journals.put(toJournalName, new ArrayList<>(journal));
        }
        return true;
    }

    private boolean delete(String journalName) {
        deleteCount++;
        journals.remove(journalName);
        return true;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("readCount").value(readCount);
        dumper.forKey("appendCount").value(appendCount).compactPrevious();
        dumper.forKey("copyCount").value(copyCount).compactPrevious();
        dumper.forKey("deleteCount").value(deleteCount).compactPrevious();
        dumper.forKey("sessions").value(journals.size());
        for (Entry<String, List<byte[]>> entry : journals.entrySet()) {
            Dumper childDumper = dumper.getChildDumper();
            childDumper.title("Session");
            childDumper.forKey("name").value(entry.getKey());
            childDumper.forKey("operations").value(entry.getValue().size()).compactPrevious();
        }
    }
}
