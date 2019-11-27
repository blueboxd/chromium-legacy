// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Append;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Copy;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Delete;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A mutation described by a journal name and a sequence of {@link JournalOperation} instances. This
 * class is used to commit changes to a journal in {@link JournalStorage}.
 */
public final class JournalMutation {
    private final String mJournalName;
    private final List<JournalOperation> mOperations;

    private JournalMutation(String journalName, List<JournalOperation> operations) {
        this.mJournalName = journalName;
        this.mOperations = Collections.unmodifiableList(operations);
    }

    /** An unmodifiable list of operations to be committed by {@link JournalStorage}. */
    public List<JournalOperation> getOperations() {
        return mOperations;
    }

    /** The name of the journal this mutation should be applied to. */
    public String getJournalName() {
        return mJournalName;
    }

    /**
     * Creates a {@link JournalMutation}, which can be used to apply mutations to a journal in the
     * underlying {@link JournalStorage}.
     */
    public static final class Builder {
        private final ArrayList<JournalOperation> mOperations = new ArrayList<>();
        private final String mJournalName;

        public Builder(String journalName) {
            this.mJournalName = journalName;
        }

        /**
         * Appends {@code value} to the journal in {@link JournalStorage}.
         *
         * <p>This method can be called repeatedly to append multiple times.
         */
        public Builder append(byte[] value) {
            mOperations.add(new Append(value));
            return this;
        }

        /** Copies the journal to {@code toJournalName}. */
        public Builder copy(String toJournalName) {
            mOperations.add(new Copy(toJournalName));
            return this;
        }

        /** Deletes the journal. */
        public Builder delete() {
            mOperations.add(new Delete());
            return this;
        }

        /** Creates a {@link JournalMutation} based on the operations added in this builder. */
        public JournalMutation build() {
            return new JournalMutation(mJournalName, new ArrayList<>(mOperations));
        }
    }
}
