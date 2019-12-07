// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.conformance.storage;

import static com.google.common.truth.Truth.assertThat;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Test;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.testing.RequiredConsumer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;

import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.List;

/**
 * Conformance test for {@link JournalStorage}. Hosts who wish to test against this should extend
 * this class and set {@code storage} to the Host implementation.
 */
public abstract class JournalStorageConformanceTest {
    private static final String JOURNAL_NAME = "journal name";
    private static final String JOURNAL_COPY_NAME = "journal copy name";
    private static final byte[] DATA_0 = "data 0".getBytes(Charset.forName("UTF-8"));
    private static final byte[] DATA_1 = "data 1".getBytes(Charset.forName("UTF-8"));

    private final Consumer<Result<List<String>>> mIsJournal1 = result -> {
        assertThat(result.isSuccessful()).isTrue();
        List<String> input = result.getValue();
        assertThat(input).hasSize(1);
        assertThat(input).contains(JOURNAL_NAME);
    };
    private final Consumer<Result<List<String>>> mIsJournal1AndJournal2 = result -> {
        assertThat(result.isSuccessful()).isTrue();
        List<String> input = result.getValue();
        assertThat(input).hasSize(2);
        assertThat(input).contains(JOURNAL_NAME);
        assertThat(input).contains(JOURNAL_COPY_NAME);
    };

    private final Consumer<Result<List<byte[]>>> mIsData0AndData1 = result -> {
        assertThat(result.isSuccessful()).isTrue();
        List<byte[]> input = result.getValue();
        // We should get back one byte array, containing the bytes of DATA_0 and DATA_1.
        assertThat(input).isNotNull();
        assertThat(input).hasSize(2);
        assertThat(Arrays.equals(input.get(0), DATA_0)).isTrue();
        assertThat(Arrays.equals(input.get(1), DATA_1)).isTrue();
    };
    private final Consumer<Result<List<byte[]>>> mIsEmptyList = result -> {
        assertThat(result.isSuccessful()).isTrue();
        List<byte[]> input = result.getValue();

        // The result should be an empty List.
        assertThat(input).isNotNull();
        assertThat(input).isEmpty();
    };
    private final Consumer<CommitResult> mIsSuccess =
            input -> assertThat(input).isEqualTo(CommitResult.SUCCESS);
    private final Consumer<Result<Boolean>> mIsFalse = result -> {
        assertThat(result.isSuccessful()).isTrue();
        Boolean input = result.getValue();
        assertThat(input).isFalse();
    };
    private final Consumer<Result<Boolean>> mIsTrue = result -> {
        assertThat(result.isSuccessful()).isTrue();
        Boolean input = result.getValue();
        assertThat(input).isTrue();
    };

    protected JournalStorage mJournalStorage;

    @Test
    public void readOfEmptyJournalReturnsEmptyData() {
        // Try to read some blobs from an empty journal store.
        RequiredConsumer<Result<List<byte[]>>> readConsumer = new RequiredConsumer<>(mIsEmptyList);
        mJournalStorage.read(JOURNAL_NAME, readConsumer);
        assertThat(readConsumer.isCalled()).isTrue();
    }

    @Test
    public void appendToJournal() {
        // Write some data and put the result from the callback into commitResult.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Read the data back into blobs.
        RequiredConsumer<Result<List<byte[]>>> readConsumer =
                new RequiredConsumer<>(mIsData0AndData1);
        mJournalStorage.read(JOURNAL_NAME, readConsumer);
        assertThat(readConsumer.isCalled()).isTrue();
    }

    @Test
    public void copyJournal() {
        // Write some data.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Copy the data into a new journal and put the result from the callback into commitResult.
        commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).copy(JOURNAL_COPY_NAME).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Read the data back into blobs.
        RequiredConsumer<Result<List<byte[]>>> readConsumer =
                new RequiredConsumer<>(mIsData0AndData1);
        mJournalStorage.read(JOURNAL_COPY_NAME, readConsumer);
        assertThat(readConsumer.isCalled()).isTrue();
    }

    @Test
    public void deleteJournal() {
        // Write some data.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Delete the journal and put the result from the callback into commitResult.
        commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).delete().build(), commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Try to read the deleted journal.
        RequiredConsumer<Result<List<byte[]>>> readConsumer = new RequiredConsumer<>(mIsEmptyList);
        mJournalStorage.read(JOURNAL_NAME, readConsumer);
        assertThat(readConsumer.isCalled()).isTrue();
    }

    @Test
    public void deleteAllJournals() {
        // Write some data, then copy into two journals.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_NAME)
                                       .append(DATA_0)
                                       .append(DATA_1)
                                       .copy(JOURNAL_COPY_NAME)
                                       .build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Delete all journals
        commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.deleteAll(commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Try to read the deleted journals
        RequiredConsumer<Result<Boolean>> existsConsumer = new RequiredConsumer<>(mIsFalse);
        mJournalStorage.exists(JOURNAL_NAME, existsConsumer);
        assertThat(existsConsumer.isCalled()).isTrue();
        existsConsumer = new RequiredConsumer<>(mIsFalse);
        mJournalStorage.exists(JOURNAL_COPY_NAME, existsConsumer);
        assertThat(existsConsumer.isCalled()).isTrue();
    }

    @Test
    public void exists() {
        // Write some data.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<Boolean>> existsConsumer = new RequiredConsumer<>(mIsTrue);
        mJournalStorage.exists(JOURNAL_NAME, existsConsumer);
        assertThat(existsConsumer.isCalled()).isTrue();
    }

    @Test
    public void exists_doesNotExist() {
        RequiredConsumer<Result<Boolean>> existsConsumer = new RequiredConsumer<>(mIsFalse);
        mJournalStorage.exists(JOURNAL_NAME, existsConsumer);
        assertThat(existsConsumer.isCalled()).isTrue();
    }

    @Test
    public void getAllJournals_singleJournal() {
        // Write some data.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(DATA_0).append(DATA_1).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<List<String>>> getAllJournalsConsumer =
                new RequiredConsumer<>(mIsJournal1);
        mJournalStorage.getAllJournals(getAllJournalsConsumer);
        assertThat(getAllJournalsConsumer.isCalled()).isTrue();
    }

    @Test
    public void getAllJournals_multipleJournals() {
        // Write some data.
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_NAME)
                                       .append(DATA_0)
                                       .append(DATA_1)
                                       .copy(JOURNAL_COPY_NAME)
                                       .build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<List<String>>> getAllJournalsConsumer =
                new RequiredConsumer<>(mIsJournal1AndJournal2);
        mJournalStorage.getAllJournals(getAllJournalsConsumer);
        assertThat(getAllJournalsConsumer.isCalled()).isTrue();
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
        RequiredConsumer<CommitResult> commitConsumer = new RequiredConsumer<>(mIsSuccess);
        mJournalStorage.commit(
                new JournalMutation.Builder(JOURNAL_NAME).append(action.toByteArray()).build(),
                commitConsumer);
        assertThat(commitConsumer.isCalled()).isTrue();

        // Ensure that it can be parsed back correctly
        RequiredConsumer<Result<List<byte[]>>> readConsumer = new RequiredConsumer<>(result -> {
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
        });
        mJournalStorage.read(JOURNAL_NAME, readConsumer);
        assertThat(readConsumer.isCalled()).isTrue();
    }
}
