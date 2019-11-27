// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.conformance.storage;

import static com.google.common.truth.Truth.assertThat;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Test;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;

import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.List;

/**
 * Conformance test for {@link JournalStorage}. Hosts who wish to test against this should extend
 * this class and set {@code storage} to the Host implementation.
 */
public abstract class JournalStorageDirectConformanceTest {
    private static final String JOURNAL_NAME = "journal name";
    private static final String JOURNAL_COPY_NAME = "journal copy name";
    private static final byte[] DATA_0 = "data 0".getBytes(Charset.forName("UTF-8"));
    private static final byte[] DATA_1 = "data 1".getBytes(Charset.forName("UTF-8"));

    protected JournalStorageDirect mJournalStorage;

    @Test
    public void readOfEmptyJournalReturnsEmptyData() {
        // Try to read some blobs from an empty journal store.
        Result<List<byte[]>> result = mJournalStorage.read(JOURNAL_NAME);
        List<byte[]> input = result.getValue();

        // The result should be an empty List.
        assertThat(input).isNotNull();
        assertThat(input).isEmpty();
    }

    @Test
    public void appendToJournal() {
        // Write some data
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Read the data back into blobs.
        Result<List<byte[]>> result = mJournalStorage.read(JOURNAL_NAME);
        List<byte[]> input = result.getValue();
        // We should get back one byte array, containing the bytes of DATA_0 and DATA_1.
        assertThat(input).isNotNull();
        assertThat(input).hasSize(2);
        assertThat(Arrays.equals(input.get(0), DATA_0)).isTrue();
        assertThat(Arrays.equals(input.get(1), DATA_1)).isTrue();
    }

    @Test
    public void copyJournal() {
        // Write some data.
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Copy the data into a new journal
        commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).copy(JOURNAL_COPY_NAME).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Read the data back into blobs.
        Result<List<byte[]>> result = mJournalStorage.read(JOURNAL_COPY_NAME);
        List<byte[]> input = result.getValue();
        // We should get back one byte array, containing the bytes of DATA_0 and DATA_1.
        assertThat(input).isNotNull();
        assertThat(input).hasSize(2);
        assertThat(Arrays.equals(input.get(0), DATA_0)).isTrue();
        assertThat(Arrays.equals(input.get(1), DATA_1)).isTrue();
    }

    @Test
    public void deleteJournal() {
        // Write some data.
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Delete the journal
        commitResult =
                mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_NAME).delete().build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Try to read the deleted journal.
        Result<List<byte[]>> result = mJournalStorage.read(JOURNAL_NAME);
        List<byte[]> input = result.getValue();

        // The result should be an empty List.
        assertThat(input).isNotNull();
        assertThat(input).isEmpty();
    }

    @Test
    public void deleteAllJournals() {
        // Write some data, then copy into two journals.
        CommitResult commitResult = mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_NAME)
                                                                   .append(DATA_0)
                                                                   .append(DATA_1)
                                                                   .copy(JOURNAL_COPY_NAME)
                                                                   .build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Delete all journals
        commitResult = mJournalStorage.deleteAll();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Try to read the deleted journals
        Result<Boolean> result = mJournalStorage.exists(JOURNAL_NAME);
        Boolean input = result.getValue();
        assertThat(input).isFalse();

        result = mJournalStorage.exists(JOURNAL_COPY_NAME);
        input = result.getValue();
        assertThat(input).isFalse();
    }

    @Test
    public void exists() {
        // Write some data.
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<Boolean> result = mJournalStorage.exists(JOURNAL_NAME);
        Boolean input = result.getValue();
        assertThat(input).isTrue();
    }

    @Test
    public void exists_doesNotExist() {
        Result<Boolean> result = mJournalStorage.exists(JOURNAL_NAME);
        Boolean input = result.getValue();
        assertThat(input).isFalse();
    }

    @Test
    public void getAllJournals_singleJournal() {
        // Write some data.
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<String>> result = mJournalStorage.getAllJournals();
        List<String> input = result.getValue();
        assertThat(input).hasSize(1);
        assertThat(input).contains(JOURNAL_NAME);
    }

    @Test
    public void getAllJournals_multipleJournals() {
        // Write some data.
        CommitResult commitResult = mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_NAME)
                                                                   .append(DATA_0)
                                                                   .append(DATA_1)
                                                                   .copy(JOURNAL_COPY_NAME)
                                                                   .build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<String>> result = mJournalStorage.getAllJournals();
        List<String> input = result.getValue();
        assertThat(input).hasSize(2);
        assertThat(input).contains(JOURNAL_NAME);
        assertThat(input).contains(JOURNAL_COPY_NAME);
    }

    @Test
    public void streamLocalAction_roundTrip() {
        // Write a Stream action with known breaking data ([INTERNAL LINK])
        StreamLocalAction action =
                StreamLocalAction.newBuilder()
                        .setTimestampSeconds(1532979950)
                        .setAction(1)
                        .setFeatureContentId("FEATURE::stories.f::5726498306727238903")
                        .build();
        CommitResult commitResult = mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(action.toByteArray()).build());
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        // Ensure that it can be parsed back correctly
        Result<List<byte[]>> result = mJournalStorage.read(JOURNAL_NAME);
        List<byte[]> bytes = result.getValue();
        assertThat(bytes).hasSize(1);
        StreamLocalAction parsedAction = null;
        try {
            parsedAction = StreamLocalAction.parseFrom(bytes.get(0));
        } catch (InvalidProtocolBufferException e) {
            throw new AssertionError(
                    "Should be able to parse StreamLocalAction bytes correctly", e);
        }
        assertThat(parsedAction).isEqualTo(action);
    }
}
